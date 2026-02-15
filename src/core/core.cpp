#include "core.hpp"

#include <cstring>

namespace kafkax {

    namespace detail {
        template <class T>
        SPSCRing<T>::SPSCRing(std::size_t cap)
            : cap_(cap),
              mask_(capacity() - 1),
              buf_(static_cast<T*>(::operator new[](sizeof(T) *cap))) {}

        template <class T>
        SPSCRing<T>::~SPSCRing() {
            T tmp;
            while (try_pop(tmp)) {}
            ::operator delete[](buf_);
        }

        template <class T>
        bool SPSCRing<T>::try_push(T&& v) {
            auto t = tail_.load(std::memory_order_relaxed);
            auto h = head_.load(std::memory_order_acquire);

            if ((t - h) >= cap_) return false;

            new (&buf_[t & mask_]) T(std::move(v));
            tail_.store(t + 1, std::memory_order_release);
            return true;
        }

        template <class T>
        bool SPSCRing<T>::try_pop(T& out) {
            auto h = head_.load(std::memory_order_relaxed);
            auto t = tail_.load(std::memory_order_acquire);

            if (h == t) return false;

            T* slot = &buf_[h & mask_];
            out = std::move(*slot);
            slot->~T();

            head_.store(h + 1, std::memory_order_release);
            return true;
        }

        template <class T>
        std::size_t SPSCRing<T>::size() const noexcept {
            return tail_.load() - head_.load();
        }

        template class SPSCRing<std::unique_ptr<Core::RawMsg>>;
        template class SPSCRing<std::unique_ptr<Event>>;

    } // namespace detail


    /* ============================================================
     * ======================  Core  ===============================
     * ============================================================ */
    Core::Core(const Config& cfg)
    : cfg_(cfg) {

        high_watermark_ =
            static_cast<std::size_t>(cfg_.raw_queue_size * cfg_.high_watermark_ratio);

        low_watermark_ =
            static_cast<std::size_t>(cfg_.raw_queue_size * cfg_.low_watermark_ratio);

        conf_ = rd_kafka_conf_new();

        rd_kafka_conf_set_rebalance_cb(
            conf_,
            [](rd_kafka_t* rk,
               rd_kafka_resp_err_t err,
               rd_kafka_topic_partition_list_t* partitions,
               void* opaque) {

                auto* self = static_cast<Core*>(opaque);
                std::lock_guard<std::mutex> lk(self->assign_mu_);

                if (err == RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS) {
                    rd_kafka_assign(rk, partitions);

                    if (self->assignment_)
                        rd_kafka_topic_partition_list_destroy(self->assignment_);

                    self->assignment_ =
                        rd_kafka_topic_partition_list_copy(partitions);
                }
                else {
                    rd_kafka_assign(rk, nullptr);

                    if (self->assignment_) {
                        rd_kafka_topic_partition_list_destroy(self->assignment_);
                        self->assignment_ = nullptr;
                    }
                }
            });

        rd_kafka_conf_set_opaque(conf_, this);
    }

    Core::~Core() {
        stop();
    }

    /* ============================================================
     * ======================  Kafka Config =======================
     * ============================================================ */

    int Core::set_conf(const std::string& key,
                   const std::string& value,
                   std::string& err) {

        char buf[256];
        auto r = rd_kafka_conf_set(conf_,
                                   key.c_str(),
                                   value.c_str(),
                                   buf,
                                   sizeof(buf));
        if (r != RD_KAFKA_CONF_OK) {
            err = buf;
            return -1;
        }
        return 0;
    }


    int Core::subscribe(const std::vector<std::string>& topics,
                        std::string& err)
    {
        rk_ = rd_kafka_new(RD_KAFKA_CONSUMER,
                           conf_,
                           nullptr,
                           0);

        if (!rk_) {
            err = "rd_kafka_new failed";
            return -1;
        }

        rd_kafka_poll_set_consumer(rk_);

        auto* list =
            rd_kafka_topic_partition_list_new(topics.size());

        for (auto& t : topics)
            rd_kafka_topic_partition_list_add(
                list,
                t.c_str(),
                RD_KAFKA_PARTITION_UA);

        auto r = rd_kafka_subscribe(rk_, list);
        rd_kafka_topic_partition_list_destroy(list);

        if (r != RD_KAFKA_RESP_ERR_NO_ERROR) {
            err = rd_kafka_err2str(r);
            return -1;
        }

        start();
        return 0;
    }

    void Core::start()
    {
        raw_qs_.resize(cfg_.decode_threads);
        evt_qs_.resize(cfg_.decode_threads);
        raw_epochs_.resize(cfg_.decode_threads);

        for (std::size_t i = 0; i < cfg_.decode_threads; ++i) {
            raw_qs_[i] = std::make_unique<
                detail::SPSCRing<std::unique_ptr<RawMsg>>>(
                cfg_.raw_queue_size);

            evt_qs_[i] = std::make_unique<
                detail::SPSCRing<std::unique_ptr<Event>>>(
                cfg_.evt_queue_size);

            raw_epochs_[i] =
                std::make_unique<std::atomic<uint64_t>>(0);

            workers_.emplace_back(
                &Core::decode_loop,
                this,
                i);
        }

        consumer_th_ =
            std::thread(&Core::consumer_loop, this);
    }

    void Core::stop()
    {
        stop_.store(true, std::memory_order_release);

        for (auto& e : raw_epochs_) {
            e->fetch_add(1);
            std::atomic_notify_all(e.get());
        }

        if (consumer_th_.joinable())
            consumer_th_.join();

        for (auto& w : workers_)
            if (w.joinable())
                w.join();

        if (rk_) {
            rd_kafka_consumer_close(rk_);
            rd_kafka_destroy(rk_);
        }

        if (assignment_) {
            rd_kafka_topic_partition_list_destroy(assignment_);
            assignment_ = nullptr;
        }
    }

    /* ============================================================
     * ======================  Consumer Loop ======================
     * ============================================================ */
    void Core::consumer_loop() {

        while (!stop_.load(std::memory_order_acquire)) {

            /* Resume requested */
            if (paused_.load(std::memory_order_acquire) &&
                resume_requested_.exchange(false))
            {
                std::lock_guard<std::mutex> lk(assign_mu_);
                if (assignment_) {
                    rd_kafka_resume_partitions(rk_, assignment_);
                    paused_.store(false, std::memory_order_release);
                }
            }

            auto* msg = rd_kafka_consumer_poll(rk_, 100);

            if (!msg) continue;

            if (msg->err) {
                rd_kafka_message_destroy(msg);
                continue;
            }

            Envelope env;
            env.topic = rd_kafka_topic_name(msg->rkt);

            env.payload.assign(
                (uint8_t*)msg->payload,
                (uint8_t*)msg->payload + msg->len);

            auto raw = std::make_unique<RawMsg>();
            raw->env = std::move(env);

            auto worker = next_worker(raw->env);

            /* Backpressure push (blocking) */
            while (!raw_qs_[worker]->try_push(std::move(raw))) {

                maybe_pause();

                auto& epoch = *raw_epochs_[worker];
                auto seen = epoch.load();
                std::atomic_wait(&epoch, seen);

                if (stop_.load())
                    break;
            }

            auto& epoch = *raw_epochs_[worker];
            epoch.fetch_add(1);
            std::atomic_notify_one(&epoch);

            total_raw_.fetch_add(1);
            rd_kafka_message_destroy(msg);

            maybe_pause();
        }
    }

    /* ============================================================
     * ======================  Decode Loop ========================
     * ============================================================ */
    void Core::decode_loop(std::size_t id)
    {
        auto& rq = *raw_qs_[id];
        auto& eq = *evt_qs_[id];
        auto& epoch = *raw_epochs_[id];

        while (!stop_.load(std::memory_order_acquire)) {

            std::unique_ptr<RawMsg> raw;

            if (!rq.try_pop(raw)) {
                auto seen = epoch.load();
                std::atomic_wait(&epoch, seen);
                continue;
            }

            epoch.fetch_add(1);
            std::atomic_notify_one(&epoch);

            total_raw_.fetch_sub(1, std::memory_order_relaxed);

            /* If below low watermark → request resume */
            if (paused_.load(std::memory_order_acquire) &&
                total_raw_.load(std::memory_order_relaxed) <= low_watermark_)
            {
                resume_requested_.store(true, std::memory_order_release);
            }

            auto ev = std::make_unique<Event>();

            ev->topic = raw->env.topic;

            kafkax_decode_result_t result{};
            kafkax_envelope_t cenv{};

            cenv.topic = raw->env.topic.c_str();
            cenv.payload = raw->env.payload.data();
            cenv.payload_len = raw->env.payload.size();

            auto fn = registry_.get_fn(raw->env.topic);

            if (!fn) {
                ev->kind = Event::Kind::Error;
                std::strncpy(
                    ev->err_msg,
                    "decoder not bound",
                    sizeof(ev->err_msg));
            } else {
                int rc = fn(&cenv, &result);
                if (rc != 0 || result.kind != 0) {
                    ev->kind = Event::Kind::Error;
                    std::strncpy(
                        ev->err_msg,
                        result.err_msg,
                        sizeof(ev->err_msg));
                } else {
                    ev->kind = Event::Kind::Data;
                    ev->data.assign(
                        result.bytes,
                        result.bytes + result.len);
                }
            }

            /* Blocking event push */
            while (!eq.try_push(std::move(ev))) {
                std::this_thread::yield();
                if (stop_.load())
                    break;
            }
        }
    }

    void Core::maybe_pause() {
        if (paused_.load(std::memory_order_acquire))
            return;

        if (total_raw_.load(std::memory_order_relaxed) < high_watermark_)
            return;

        std::lock_guard<std::mutex> lk(assign_mu_);

        if (!assignment_)
            return;

        rd_kafka_pause_partitions(rk_, assignment_);
        paused_.store(true, std::memory_order_release);
    }

    /* ============================================================
     * ======================  Control Plane ======================
     * ============================================================ */

    int Core::bind_topic(const std::string& topic,
                     const std::string& so_path,
                     const std::string& symbol,
                     std::string& err)
    {
        return registry_.bind(topic, so_path, symbol, err);
    }

    int Core::rebind_topic(const std::string& topic,
                           const std::string& so_path,
                           const std::string& symbol,
                           std::string& err)
    {
        return registry_.rebind(topic, so_path, symbol, err);
    }

    int Core::unbind_topic(const std::string& topic)
    {
        return registry_.unbind(topic);
    }

    bool Core::get_topic_decoder(const std::string& topic,
                                 DecoderRegistry::BindingInfo& out) const
    {
        return registry_.get_decoder_info(topic, out);
    }

    /* ============================================================
     * ======================  Drain ==============================
     * ============================================================ */

    void Core::drainTo(std::vector<Event>& out) {

        if (evt_qs_.empty())
            return;

        auto qn = evt_qs_.size();
        auto start = drain_rr_.fetch_add(1) % qn;

        for (std::size_t i = 0; i < qn; ++i)
        {
            auto idx = (start + i) % qn;

            std::unique_ptr<Event> ev;

            while (evt_qs_[idx]->try_pop(ev)) {
                out.push_back(std::move(*ev));
            }
        }
    }

    std::size_t Core::next_worker(const Envelope&)
    {
        return rr_.fetch_add(1) % cfg_.decode_threads;
    }

} // namespace kafkax