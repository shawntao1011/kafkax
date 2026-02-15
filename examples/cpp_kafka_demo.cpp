#include <csignal>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "core/core.hpp"

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
        if (ev.kind == kafkax::Event::Kind::Error)
            ++error;
        else
            ++data;
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

void print_event(const kafkax::Event& ev) {
    if (ev.kind == kafkax::Event::Kind::Error) {
        std::cerr << "[ERROR] topic=" << ev.topic
                  << " msg=" << ev.err_msg << std::endl;
        return;
    }

    std::cout << "[DATA] topic=" << ev.topic
              << " len=" << ev.data.size();

    if (!ev.data.empty()) {
        std::cout << " payload="
                  << std::string(ev.data.begin(), ev.data.end());
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

    kafkax::Core core(kafkax::Core::Config{1, 8192, 8192});
    std::string err;

    auto set = [&](const char* key, const char* value) {
        if (core.set_conf(key, value, err) != 0) {
            std::cerr << "set(" << key << ", " << value
                      << ") failed: " << err << std::endl;
            return false;
        }
        return true;
    };

    if (!set("bootstrap.servers", "192.168.2.209:9092") ||
        !set("group.id", "momo-subpb-dev") ||
        !set("enable.auto.commit", "true") ||
        !set("auto.offset.reset", "earliest")) {
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

    std::cout << "kafkax drainTo demo started. topic=" << topic
              << " decoder=" << decoder_lib << ":" << decoder_fn
              << " (Ctrl+C to stop)" << std::endl;

    Stats stats;

    while (g_running) {

        std::vector<kafkax::Event> out;
        core.drainTo(out);

        for (const auto& ev : out) {
            stats.on_event(ev);

            //print_event(ev);
        }

        stats.maybe_print();

        if (out.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    std::cout << "\nFinal Stats:\n"
              << "total=" << stats.total
              << " data=" << stats.data
              << " error=" << stats.error
              << std::endl;

    std::cout << "kafkax drainTo demo stopped" << std::endl;
    return 0;
}