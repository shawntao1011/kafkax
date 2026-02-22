#pragma once
#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <librdkafka/rdkafka.h>

#include "event.h"
#include "kafkax/decoder_registry.hpp"

namespace kafkax {

    namespace detail {
        template <class T>
        class SPSCRing {
        public:
            explicit SPSCRing(std::size_t capacity);

            ~SPSCRing();

            SPSCRing(const SPSCRing&) = delete;
            SPSCRing& operator=(const SPSCRing&) = delete;

            bool try_push(T&& v);
            bool try_pop(T& out);

            std::size_t capacity() const noexcept { return cap_; }
            std::size_t size() const noexcept;

        private:
            const std::size_t cap_;
            T* buf_{nullptr};

            alignas(64) std::atomic<std::uint64_t> head_{0}; // consumer
            alignas(64) std::atomic<std::uint64_t> tail_{0}; // producer
        };
    } // namespace kafkax::detail

    class Core {
    public:
        struct KafkaConfig {
            std::string bootstrap_servers{};
            std::string group_id{};
            bool enable_auto_commit{true};
            std::string auto_offset_reset{"latest"};

            std::unordered_map<std::string, std::string> extra{};
        };

        struct DecodeConfig {
            std::size_t decode_threads{4};
            std::size_t raw_queue_size{8192};
            std::size_t evt_queue_size{8192};

            double high_watermark_ratio{0.9};
            double low_watermark_ratio{0.5};
        };

        struct RawMsg {
            rd_kafka_message_t* msg{nullptr};

            ~RawMsg() {
                if (msg) {
                    rd_kafka_message_destroy(msg);
                    msg = nullptr;
                }
            }
        };

        using DrainFn = void(*)(void* user, const Event& ev);

        explicit Core(const DecodeConfig& cfg);
        Core(const DecodeConfig& cfg, const KafkaConfig& kafka_cfg);
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
        void drainTo(std::vector<Event>& out, std::size_t limit = 4096);

        int notify_fd() const noexcept { return efd_; }

    private:
        int apply_kafka_config(const KafkaConfig& kafka_cfg, std::string& err);

        void start();
        void stop();

        void consumer_loop();
        void decode_loop(std::size_t worker_id);

        void maybe_pause();

        std::size_t next_worker(const rd_kafka_message_t* msg);

    private:
        DecodeConfig cfg_;

        /* Kafka */
        rd_kafka_t* rk_{nullptr};
        rd_kafka_conf_t* conf_{nullptr};

        bool kafka_conf_ok_{true};
        std::string kafka_conf_err_;

        std::atomic<bool> stop_{false};

        std::thread consumer_th_;
        std::vector<std::thread> workers_;

        /* Queues */
        std::vector<std::unique_ptr<detail::SPSCRing<std::unique_ptr<RawMsg>>>> raw_qs_;
        std::vector<std::unique_ptr<detail::SPSCRing<std::unique_ptr<Event>>>> evt_qs_;

        /* Epochs for atomic_wait */
        std::vector<std::unique_ptr<std::atomic<std::uint64_t>>> raw_epochs_;

        /* Global counters for watermarks */
        std::atomic<std::size_t> total_raw_{0};

        std::size_t high_watermark_;
        std::size_t low_watermark_;

        std::atomic<bool> paused_{false};
        std::atomic<bool> resume_requested_{false};

        std::atomic<std::size_t> rr_{0};
        std::atomic<std::size_t> drain_rr_{0};

        mutable std::mutex assign_mu_;
        rd_kafka_topic_partition_list_t* assignment_{nullptr};

        DecoderRegistry registry_;

        int efd_{-1};                                 // eventfd for sd1 wakeup
        std::atomic<bool> evt_notified_{false};     // coalesce notify (armed flag)
    };

} // namespace kafkax