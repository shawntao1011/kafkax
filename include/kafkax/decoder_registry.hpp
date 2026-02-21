#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "kafkax/decoder.h"

namespace kafkax {

    struct Router {
        std::unordered_map<std::string, kafkax_decode_fn> table;

        kafkax_decode_fn lookup(const std::string& topic) const {
            auto it = table.find(topic);
            if (it == table.end()) return nullptr;
            return it->second;
        }
    };

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
        DecoderRegistry();
        ~DecoderRegistry();

        bool bind(const std::string& topic,
                  const std::string& so_path,
                  const std::string& decoder_name,
                  std::string& err);

        /* Rebind topic (replace existing) */
        int rebind(const std::string& topic,
                   const std::string& so_path,
                   const std::string& symbol,
                   std::string& err);

        /* Remove binding */
        int unbind(const std::string& topic);

        kafkax_decode_fn get_fn(const std::string& topic) const;

        /* Combined mode: preload bindings from a simple config file.
         * Format: <topic><space><symbol_or_alias>
         * - symbol_or_alias may be a real exported function name (e.g. decode_basicqot)
         * - or an alias (e.g. basicqot) which will be tried as decode_<alias>
         * - or '-' meaning use anchor (kafkax_decoder_entry)
         */
        int bind_from_file(const std::string& so_path,
                           const std::string& conf_path,
                           std::string& err);

    private:
        std::atomic<std::shared_ptr<const Router>> router_;

        struct PluginHandle {
            void* handle;
            std::string so_path;
        };

        std::vector<PluginHandle> loaded_plugins_;
    };

} // namespace kafkax