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
            return it == table.end() ? nullptr : it->second;
        }
    };

    class DecoderRegistry {
    public:
        struct BindingInfo {
            std::string so_path;
            std::string symbol;
        };

        DecoderRegistry();
        ~DecoderRegistry();

        int bind(const std::string& topic,
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

        bool get_decoder_info(const std::string& topic, BindingInfo& out) const;
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
        struct PluginHandle {
            void* handle;
            std::string so_path;
        };

        struct BindingEntry {
            std::size_t plugin_idx{0};
            BindingInfo info;
        };

        int ensure_plugin_loaded(const std::string& so_path,
                         std::size_t& plugin_idx,
                         std::string& err);

        int resolve_symbol(std::size_t plugin_idx,
                           const std::string& symbol,
                           kafkax_decode_fn& fn,
                           std::string& err) const;

    private:
        std::atomic<std::shared_ptr<const Router>> router_;
        mutable std::mutex mu_;

        std::vector<PluginHandle> loaded_plugins_;
        std::unordered_map<std::string, std::size_t> so_to_plugin_;
        std::unordered_map<std::string, BindingEntry> topic_bindings_;
    };

} // namespace kafkax