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

    struct Args {
    std::string bootstrap_servers = "127.0.0.1:9092";
    std::string topic = "momo-subpb-dev";
    std::string group_id = "momo-subpb-dev";
    std::string decoder_lib = "libkafkax_default_decoder.so";
    std::string decoder_fn = "kafkax_default_decoder";
};

void print_usage(const char* prog) {
    std::cout
        << "Usage:\n"
        << "  " << prog
        << " <bootstrap_servers> [topic] [group_id] [decoder_lib] [decoder_fn]\n\n"
        << "Examples:\n"
        << "  " << prog << " 127.0.0.1:9092\n"
        << "  " << prog
        << " 127.0.0.1 momo.orderbook test.demo"
           "libkafkax_default_decoder.so kafkax_default_decoder\n\n"
        << "Defaults (when optional args are omitted):\n"
        << "  topic       = momo.orderbook\n"
        << "  group_id    = test.demo\n"
        << "  decoder_lib = libkafkax_default_decoder.so\n"
        << "  decoder_fn  = kafkax_default_decoder\n";
}

enum class ParseResult { Ok, ShowHelp, Invalid };

ParseResult parse_args(int argc, char** argv, Args& out) {
    if (argc < 2) {
        print_usage(argv[0]);
        return ParseResult::Invalid;
    }

    const std::string first = argv[1] ? argv[1] : "";
    if (first == "-h" || first == "--help") {
        print_usage(argv[0]);
        return ParseResult::ShowHelp;
    }

    out.bootstrap_servers = argv[1];
    if (argc > 2) out.topic = argv[2];
    if (argc > 3) out.group_id = argv[3];
    if (argc > 4) out.decoder_lib = argv[4];
    if (argc > 5) out.decoder_fn = argv[5];

    return ParseResult::Ok;
}

struct Stats {
    std::uint64_t total = 0;
    std::uint64_t data = 0;
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

    Args args;
    const ParseResult parse_result = parse_args(argc, argv, args);
    if (parse_result == ParseResult::ShowHelp) {
        return 0;
    }
    if (parse_result == ParseResult::Invalid) {
        return 1;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::string err;

    kafkax::Core::DecodeConfig decode_cfg{4, 8192, 8192};

    kafkax::Core::KafkaConfig kafka_cfg{};
    kafka_cfg.bootstrap_servers = args.bootstrap_servers;
    kafka_cfg.group_id = args.group_id;
    kafka_cfg.enable_auto_commit = true;
    kafka_cfg.auto_offset_reset = "earliest";

    kafkax::Core core(decode_cfg, kafka_cfg);
    if (!err.empty()) {
        std::cerr << "Core(core_cfg, kafka_cfg) failed: " << err << std::endl;
        return 1;
    }

    if (core.bind_topic(args.topic, args.decoder_lib, args.decoder_fn, err) !=
        0) {
        std::cerr << "bind_topic failed: " << err << std::endl;
        return 1;
    }

    if (core.subscribe({args.topic}, err) != 0) {
        std::cerr << "subscribe failed: " << err << std::endl;
        return 1;
    }

    const int efd = core.notify_fd();
    if (efd < 0) {
        std::cerr << "notify_fd invalid: " << efd << std::endl;
        return 1;
    }

    std::cout << "kafkax drainTo demo started. brokers=" << args.bootstrap_servers
              << " topic=" << args.topic << " group_id=" << args.group_id
              << " decoder=" << args.decoder_lib << ":" << args.decoder_fn
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
                // print_event(ev);
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