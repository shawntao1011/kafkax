#pragma once
#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

#include <librdkafka/rdkafka.h>

#include "decoder_registry.hpp"

namespace kafkax {

    namespace detail {
        template <class T>
        class SPSCRing {
        public:
            explicit SPSCRing(std::size_t capacity_pow2);

            ~SPSCRing();

            SPSCRing(const SPSCRing&) = delete;
            SPSCRing& operator=(const SPSCRing&) = delete;

            bool try_push(T&& v);
            bool try_pop(T& out);

            std::size_t capacity() const noexcept { return cap_; }
            std::size_t size() const noexcept;

        private:
            const std::size_t cap_;
            const std::size_t mask_;
            T* buf_{nullptr};

            alignas(64) std::atomic<std::uint64_t> head_{0}; // consumer
            alignas(64) std::atomic<std::uint64_t> tail_{0}; // producer
        };
    } // namespace kafkax::detail


    /* --------------------------
     * Input envelope (internal)
     * -------------------------- */
    struct Envelope {
        std::string topic;
        std::vector<std::uint8_t> key;
        std::vector<std::uint8_t> payload;
        std::int64_t ingest_ns{0};
    };

    /* --------------------------
     * Output event (core result)
     * -------------------------- */
    struct Event {
        enum class Kind : std::uint8_t {
            Data  = 0,
            Error = 1
        };

        Kind kind{Kind::Data};

        std::string topic;
        std::int64_t ingest_ns{0};

        std::vector<std::uint8_t> data;   // when Kind::Data
        char err_msg[96]{0};              // when Kind::Error
    };

    class Core {
    public:
        struct Config {
            std::size_t decode_threads{4};
            std::size_t raw_queue_size{8192};  // power-of-two
            std::size_t evt_queue_size{8192};  // power-of-two

            double high_watermark_ratio{0.9};
            double low_watermark_ratio{0.5};
        };

        struct RawMsg {
            Envelope env;
        };

        using DrainFn = void(*)(void* user, const Event& ev);

        explicit Core(const Config& cfg);
        ~Core();

        Core(const Core&) = delete;
        Core& operator=(const Core&) = delete;

        /* ---------- Kafka lifecycle ---------- */
        int set_conf(const std::string& key,
                     const std::string& value,
                     std::string& err);

        int subscribe(const std::vector<std::string>& topics,
                      std::string& err);

        /* ----- control plane (decoder binding) ----- */
        int bind_topic(const std::string& topic,
               const std::string& so_path,
               const std::string& symbol,
               std::string& err);

        int rebind_topic(const std::string& topic,
                 const std::string& so_path,
                 const std::string& symbol,
                 std::string& err);

        int unbind_topic(const std::string& topic);

        bool get_topic_decoder(const std::string& topic,
                               DecoderRegistry::BindingInfo& out) const;

        /* ----- data plane ----- */
        void drainTo(std::vector<Event>& out);

    private:
        void start();
        void stop();

        void consumer_loop();
        void decode_loop(std::size_t worker_id);

        void maybe_pause();
        void maybe_resume();

        std::size_t next_worker(const Envelope& env);

    private:
        Config cfg_;

        /* Kafka */
        rd_kafka_t* rk_{nullptr};
        rd_kafka_conf_t* conf_{nullptr};

        std::atomic<bool> stop_{false};

        std::thread consumer_th_;
        std::vector<std::thread> workers_;

        std::vector<std::unique_ptr<detail::SPSCRing<std::unique_ptr<RawMsg>>>> raw_qs_;
        std::vector<std::unique_ptr<detail::SPSCRing<std::unique_ptr<Event>>>>   evt_qs_;

        DecoderRegistry registry_;

        std::atomic<bool> paused_{false};

        std::size_t high_watermark_;
        std::size_t low_watermark_;

        std::atomic<std::size_t> rr_{0};
        std::atomic<std::size_t> drain_rr_{0};

        mutable std::mutex assign_mu_;
        rd_kafka_topic_partition_list_t* assignment_{nullptr};
    };

} // namespace kafkax