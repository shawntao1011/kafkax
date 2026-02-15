#include "decoder_registry.hpp"
#ifndef _WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

kafkax::DecoderRegistry::DecoderRegistry() = default;

kafkax::DecoderRegistry::~DecoderRegistry() {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [_, entry] : topic_map_) {
        if (entry->handle) {
            dlclose(entry->handle);
        }
    }
}

int kafkax::DecoderRegistry::load_decoder(
    const std::string& so_path,
    const std::string& symbol,
    std::unique_ptr<DecoderEntry>& out,
    std::string& err) {
    void* handle = dlopen(so_path.c_str(), RTLD_NOW);
    if (!handle) {
        err = dlerror();
        return -1;
    }


    /* Check ABI version */
    auto ver_fn = (kafkax_decoder_abi_version_fn)
        dlsym(handle, "kafkax_decoder_abi_version");

    if (!ver_fn) {
        err = "symbol kafkax_decoder_abi_version not found";
        dlclose(handle);
        return -2;
    }

    if (ver_fn() != KAFKAX_DECODER_ABI_VERSION) {
        err = "decoder ABI version mismatch";
        dlclose(handle);
        return -3;
    }

    auto fn = (kafkax_decode_fn)
        dlsym(handle, symbol.c_str());

    if (!fn) {
        err = "symbol " + symbol + " not found";
        dlclose(handle);
        return -4;
    }

    out = std::make_unique<DecoderEntry>();
    out->handle = handle;
    out->fn = fn;
    out->so_path = so_path;
    out->symbol = symbol;

    return 0;
}

int kafkax::DecoderRegistry::bind(const std::string& topic,
                           const std::string& so_path,
                           const std::string& symbol,
                           std::string& err)
{
    std::lock_guard<std::mutex> lk(mu_);

    if (topic_map_.count(topic)) {
        err = "topic already bound";
        return -1;
    }

    std::unique_ptr<DecoderEntry> entry;
    int rc = load_decoder(so_path, symbol, entry, err);
    if (rc != 0) return rc;

    topic_map_[topic] = std::move(entry);
    return 0;
}

int kafkax::DecoderRegistry::rebind(const std::string& topic,
                             const std::string& so_path,
                             const std::string& symbol,
                             std::string& err)
{
    std::lock_guard<std::mutex> lk(mu_);

    auto it = topic_map_.find(topic);
    if (it != topic_map_.end()) {
        if (it->second->handle)
            dlclose(it->second->handle);
        topic_map_.erase(it);
    }

    std::unique_ptr<DecoderEntry> entry;
    int rc = load_decoder(so_path, symbol,entry, err);
    if (rc != 0) return rc;

    topic_map_[topic] = std::move(entry);
    return 0;
}

int kafkax::DecoderRegistry::unbind(const std::string& topic)
{
    std::lock_guard<std::mutex> lk(mu_);

    auto it = topic_map_.find(topic);
    if (it == topic_map_.end())
        return -1;

    if (it->second->handle)
        dlclose(it->second->handle);

    topic_map_.erase(it);
    return 0;
}

bool kafkax::DecoderRegistry::get_decoder_info(const std::string& topic, BindingInfo& out) const {
    std::lock_guard<std::mutex> lk(mu_);

    auto it = topic_map_.find(topic);
    if (it == topic_map_.end())
        return false;

    out.so_path = it->second->so_path;
    out.symbol  = it->second->symbol;

    return true;
}

kafkax_decode_fn
kafkax::DecoderRegistry::get_fn(const std::string& topic) const
{
    std::lock_guard<std::mutex> lk(mu_);

    auto it = topic_map_.find(topic);
    if (it == topic_map_.end())
        return nullptr;

    return it->second->fn;
}