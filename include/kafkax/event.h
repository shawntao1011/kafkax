#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace kafkax {

    struct Event {
        enum class Kind : std::uint8_t { Data = 0, Error = 1 };

        Kind kind{Kind::Data};

        std::string topic;
        std::vector<std::uint8_t> key;
        std::int64_t ingest_ns{0};

        /* Observability: which decoder produced this. */
        std::string decoder;

        /* Success payload (decoded bytes, e.g. q kbytes later) */
        std::vector<std::uint8_t> bytes;

        /* Error message (fixed size to keep ABI-friendly patterns) */
        char err_msg[96]{0};
    };

} // namespace kafkax
