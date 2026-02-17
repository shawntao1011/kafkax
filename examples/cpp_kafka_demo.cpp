#include <csignal>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <poll.h>
#include "kafkax/core.hpp"

namespace {

volatile std::sig_atomic_t g_running = 1;

void on_signal(int) {
    g_running = 0;
}

struct Stats {
    std::uint64_t total = 0;
    std::uint64_t data  = 0;
    std::uint64_t error = 0;

    std::uint64_t last_total = 0;
    std::chrono::steady_clock::time_point last_print =
        std::chrono::steady_clock::now();

    void on_event(const kafkax::Event& ev) {
        ++total;
        if (ev.kind == kafkax::Event::Kind::Error) ++error;
        else ++data;
    }

    void maybe_print() {
        auto now = std::chrono::steady_clock::now();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - last_print).count();
        if (sec >= 1) {
            std::uint64_t delta = total - last_total;
            double qps = sec > 0 ? double(delta) / sec : 0.0;

            std::cout << "[STATS] total=" << total
                      << " data=" << data
                      << " error=" << error
                      << " qps=" << qps
                      << std::endl;

            last_total = total;
            last_print = now;
        }
    }
};

static inline void drain_eventfd(int fd) {
    // eventfd is NONBLOCK in Core; read until EAGAIN
    std::uint64_t v;
    for (;;) {
        ssize_t n = ::read(fd, &v, sizeof(v));
        if (n == (ssize_t)sizeof(v)) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        break;
    }
}

void print_event(const kafkax::Event& ev) {
    if (ev.kind == kafkax::Event::Kind::Error) {
        std::cerr << "[ERROR] topic=" << ev.topic
                  << " msg=" << ev.err_msg << std::endl;
        return;
    }

    std::cout << "[DATA] topic=" << ev.topic
              << " len=" << ev.bytes.size();

    if (!ev.bytes.empty()) {
        std::cout << " payload="
                  << std::string(ev.bytes.begin(), ev.bytes.end());
    }

    std::cout << std::endl;
}

} // namespace

int main(int argc, char** argv) {

    const std::string topic       = argc > 1 ? argv[1] : "momo-subpb-dev";
    const std::string decoder_lib = argc > 2 ? argv[2] : "libkafkax_default_decoder.so";
    const std::string decoder_fn  = argc > 3 ? argv[3] : "kafkax_default_decode";

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::string err;

    kafkax::Core::DecodeConfig decode_cfg{4, 8192, 8192};

    kafkax::Core::KafkaConfig kafka_cfg{};
    kafka_cfg.bootstrap_servers = "192.168.2.209:9092";
    kafka_cfg.group_id = "momo-subpb-dev";
    kafka_cfg.enable_auto_commit = true;
    kafka_cfg.auto_offset_reset = "earliest";

    kafkax::Core core(decode_cfg, kafka_cfg);
    if (!err.empty()) {
        std::cerr << "Core(core_cfg, kafka_cfg) failed: " << err << std::endl;
        return 1;
    }

    if (core.bind_topic(topic, decoder_lib, decoder_fn, err) != 0) {
        std::cerr << "bind_topic failed: " << err << std::endl;
        return 1;
    }

    if (core.subscribe({topic}, err) != 0) {
        std::cerr << "subscribe failed: " << err << std::endl;
        return 1;
    }

    const int efd = core.notify_fd();
    if (efd < 0) {
        std::cerr << "notify_fd invalid: " << efd << std::endl;
        return 1;
    }

    std::cout << "kafkax drainTo demo started. topic=" << topic
              << " decoder=" << decoder_lib << ":" << decoder_fn
              << " (Ctrl+C to stop)" << std::endl;

    Stats stats;

    pollfd pfd{};
    pfd.fd = efd;
    pfd.events = POLLIN;

    constexpr std::size_t LIMIT = 4096;

    while (g_running) {

        // Wait until efd readable (or timeout so we can print stats / respond to signal)
        int rc = ::poll(&pfd, 1, 1000);
        if (!g_running) break;

        if (rc < 0) {
            if (errno == EINTR) continue; // interrupted by signal
            std::perror("poll");
            break;
        }

        if (rc == 0) {
            // timeout: no events, but we can still print stats
            stats.maybe_print();
            continue;
        }

        // Clear eventfd readable state
        drain_eventfd(efd);


        // Drain batches; Core will re-notify if backlog remains
        for (;;) {
            std::vector<kafkax::Event> out;
            out.reserve(LIMIT);

            core.drainTo(out, LIMIT);
            if (out.empty()) break;

            for (const auto& ev : out) {
                stats.on_event(ev);
                // print_event(ev)
            }

            stats.maybe_print();

            // If we hit LIMIT, likely backlog remains; loop again immediately.
            if (out.size() < LIMIT) break;
        }
    }

    std::cout << "\nFinal Stats:\n"
              << "total=" << stats.total
              << " data=" << stats.data
              << " error=" << stats.error
              << std::endl;

    std::cout << "kafkax eventfd demo stopped" << std::endl;
    return 0;
}