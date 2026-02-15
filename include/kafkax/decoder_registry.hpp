#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "decoder.h"

namespace kafkax {

    /*
     * DecoderRegistry
     *
     * Responsibilities:
     *  - dlopen shared library
     *  - verify ABI version
     *  - resolve kafkax_decode symbol
     *  - bind topic -> decoder
     *  - allow rebind / unbind
     *  - query decoder info
     *
     * Thread-safe.
     */

    class DecoderRegistry {
    public:
        struct BindingInfo {
            std::string so_path;   // plugin .so path
            std::string symbol;    // decoder function name
        };

        DecoderRegistry();
        ~DecoderRegistry();

        DecoderRegistry(const DecoderRegistry&) = delete;
        DecoderRegistry& operator=(const DecoderRegistry&) = delete;

        /* Bind topic to a decoder shared library */
        int bind(const std::string& topic,
                 const std::string& so_path,
                 const std::string& symbol,
                 std::string& err);

        /* Rebind topic (replace existing) */
        int rebind(const std::string& topic,
                   const std::string& so_path,
                   const std::string& symbol,
                   std::string& err);

        /* Remove binding */
        int unbind(const std::string& topic);

        /* Lookup decoder function for topic */
        bool get_decoder_info(const std::string& topic, BindingInfo& out) const;

        kafkax_decode_fn get_fn(const std::string& topic) const;

    private:
        struct DecoderEntry {
            void* handle{nullptr};
            kafkax_decode_fn fn{nullptr};
            std::string so_path;
            std::string symbol;
        };

        mutable std::mutex mu_;
        std::unordered_map<std::string, std::unique_ptr<DecoderEntry>> topic_map_;

        int load_decoder(const std::string& so_path,
                         const std::string& symbol,
                         std::unique_ptr<DecoderEntry>& out,
                         std::string& err);
    };

} // namespace kafkax