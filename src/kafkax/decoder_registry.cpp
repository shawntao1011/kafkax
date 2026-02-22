#include "kafkax/decoder_registry.hpp"
#include <dlfcn.h>

namespace kafkax {

    DecoderRegistry::DecoderRegistry()
        : router_(std::make_shared<Router>())
    {
    }

    DecoderRegistry::~DecoderRegistry() {
        for (auto& plugin : loaded_plugins_) {
            if (plugin.handle) {
                dlclose(plugin.handle);
            }
        }
    }

    int DecoderRegistry::ensure_plugin_loaded(const std::string& so_path,
                                              std::size_t& plugin_idx,
                                              std::string& err) {
        if (auto it = so_to_plugin_.find(so_path); it != so_to_plugin_.end()) {
            plugin_idx = it->second;
            return 0;
        }

        void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            err = dlerror();
            return -1;
        }

        /* Check ABI version */
        auto abi_fn = reinterpret_cast<int (*)()>(
            dlsym(handle, "kafkax_decoder_abi_version"));
        if (!abi_fn) {
            err = "symbol kafkax_decoder_abi_version not found";
            dlclose(handle);
            return -2;
        }

        if (abi_fn() != KAFKAX_DECODER_ABI_VERSION) {
            err = "decoder ABI version mismatch";
            dlclose(handle);
            return -3;
        }

        plugin_idx = loaded_plugins_.size();
        loaded_plugins_.push_back(PluginHandle{handle, so_path});
        so_to_plugin_[so_path] = plugin_idx;
        return 0;
    }

    int DecoderRegistry::resolve_symbol(std::size_t plugin_idx,
                                    const std::string& symbol,
                                    kafkax_decode_fn& fn,
                                    std::string& err) const
    {
        if (plugin_idx >= loaded_plugins_.size() || !loaded_plugins_[plugin_idx].handle) {
            err = "invalid plugin handle";
            return -1;
        }

        auto* sym = dlsym(loaded_plugins_[plugin_idx].handle, symbol.c_str());
        if (!sym) {
            err = "decoder symbol not found: " + symbol;
            return -2;
        }

        fn = reinterpret_cast<kafkax_decode_fn>(sym);
        return 0;
    }

    int DecoderRegistry::bind(const std::string& topic,
                              const std::string& so_path,
                              const std::string& symbol,
                              std::string& err) {
        std::lock_guard<std::mutex> lk(mu_);

        std::size_t plugin_idx = 0;
        int rc = ensure_plugin_loaded(so_path, plugin_idx, err);
        if (rc != 0) {
            return rc;
        }

        kafkax_decode_fn fn = nullptr;
        rc = resolve_symbol(plugin_idx, symbol, fn, err);
        if (rc != 0) {
            return rc;
    }

    // New Router
        auto old_router = router_.load(std::memory_order_acquire);
        auto new_router = std::make_shared<Router>(*old_router);
        new_router->table[topic] = fn;
        router_.store(new_router, std::memory_order_release);

        topic_bindings_[topic] = BindingEntry{plugin_idx, BindingInfo{so_path, symbol}};
        return 0;
    }

    int DecoderRegistry::bind_builtin(const std::string& topic,
                                  const std::string& symbol,
                                  kafkax_decode_fn fn,
                                  std::string& err) {
        if (!fn) {
            err = "builtin decoder function is null";
            return -1;
        }

        std::lock_guard<std::mutex> lk(mu_);

        auto old_router = router_.load(std::memory_order_acquire);
        auto new_router = std::make_shared<Router>(*old_router);
        new_router->table[topic] = fn;
        router_.store(new_router, std::memory_order_release);

        topic_bindings_[topic] = BindingEntry{0, BindingInfo{"builtin:kafkax_core", symbol}};
        return 0;
    }

    int DecoderRegistry::rebind(const std::string& topic,
                                const std::string& so_path,
                                const std::string& symbol,
                                std::string& err)
    {
        return bind(topic, so_path, symbol, err);
    }

    int DecoderRegistry::unbind(const std::string& topic) {
        std::lock_guard<std::mutex> lk(mu_);

        auto old_router = router_.load(std::memory_order_acquire);
        auto new_router = std::make_shared<Router>(*old_router);
        new_router->table.erase(topic);
        router_.store(new_router, std::memory_order_release);

        topic_bindings_.erase(topic);
        return 0;
    }

    kafkax_decode_fn DecoderRegistry::get_fn(const std::string& topic) const
    {
        auto r = router_.load(std::memory_order_acquire);
        return r ? r->lookup(topic) : nullptr;
    }

    bool DecoderRegistry::get_decoder_info(const std::string& topic,
                                           BindingInfo& out) const
    {
        std::lock_guard<std::mutex> lk(mu_);

        auto it = topic_bindings_.find(topic);
        if (it == topic_bindings_.end()) {
            return false;
        }
        out = it->second.info;
        return true;
    }

} // namespace kafkax