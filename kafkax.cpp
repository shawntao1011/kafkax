#include "kx/k.h"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <unistd.h>
#include <errno.h>

#include "kafkax/core.hpp"

namespace {

    struct Entry {
        std::unique_ptr<kafkax::Core> core;
        int efd{-1};
        bool sd1_registered{false};
    };

    std::mutex g_mu;
    std::unordered_map<int, Entry> g_entries;    // handle -> entry
    std::unordered_map<int, int>   g_fd2handle;  // efd -> handle
    std::atomic<int> g_next_handle{1};

    static inline bool k_is_dict(K x)     { return x && x->t == 99; }
    static inline bool k_is_sym_atom(K x) { return x && x->t == -KS; }
    static inline bool k_is_sym_vec(K x)  { return x && x->t == KS; }
    static inline bool k_is_char_vec(K x) { return x && x->t == KC; }

    static inline int get_handle(K h) {
        if (!h) return -1;
        if (h->t == -KI) return h->i;
        if (h->t == -KJ) return (int)h->j;
        return -1;
    }

    static inline std::string k_to_string(K v) {
        if (!v) return {};
        if (v->t == -KS) return std::string(v->s);
        if (v->t == KC)  return std::string(kC(v), kC(v) + v->n);
        if (v->t == -KI) return std::to_string(v->i);
        if (v->t == -KJ) return std::to_string(v->j);
        if (v->t == -KB) return v->g ? "true" : "false";
        return {};
    }

    static inline bool dict_get(K d, const char* key, K& out) {
        if (!k_is_dict(d)) return false;
        K keys = kK(d)[0];
        K vals = kK(d)[1];
        if (!keys || keys->t != KS) return false;
        for (J i = 0; i < keys->n; ++i) {
            if (0 == std::strcmp(kS(keys)[i], key)) {
                out = kK(vals)[i];
                return true;
            }
        }
        return false;
    }

    static inline void drain_eventfd(int fd) {
        std::uint64_t v;
        for (;;) {
            ssize_t n = ::read(fd, &v, sizeof(v));
            if (n == (ssize_t)sizeof(v)) continue;
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            break;
        }
    }

    static inline K k_errvec(const char* s, std::size_t cap) {
        if (!s || cap == 0) return ktn(KC, 0);
        std::size_t n = ::strnlen(s, cap);
        return kpn((S)s, (J)n);  // q char vector length n
    }

    static void parse_cfg(K cfg, kafkax::Core::DecodeConfig& dcfg, kafkax::Core::KafkaConfig& kcfg)
    {
        dcfg.decode_threads = 4;
        dcfg.raw_queue_size = 8192;
        dcfg.evt_queue_size = 8192;

        kcfg.enable_auto_commit = true;
        kcfg.auto_offset_reset = "earliest";

        if (!k_is_dict(cfg)) return;

        K v = nullptr;

        // decode
        if (dict_get(cfg, "decode_threads", v) && v) {
            if (v->t == -KI) dcfg.decode_threads = (std::size_t)std::max(1, v->i);
            else if (v->t == -KJ) dcfg.decode_threads = (std::size_t)std::max<J>(1, v->j);
        }
        if (dict_get(cfg, "raw_queue_size", v) && v) {
            if (v->t == -KI) dcfg.raw_queue_size = (std::size_t)std::max(1, v->i);
            else if (v->t == -KJ) dcfg.raw_queue_size = (std::size_t)std::max<J>(1, v->j);
        }
        if (dict_get(cfg, "evt_queue_size", v) && v) {
            if (v->t == -KI) dcfg.evt_queue_size = (std::size_t)std::max(1, v->i);
            else if (v->t == -KJ) dcfg.evt_queue_size = (std::size_t)std::max<J>(1, v->j);
        }

        // kafka (accept both bootstrap.servers and metadata.broker.list)
        if (dict_get(cfg, "bootstrap.servers", v) && v) {
            kcfg.bootstrap_servers = k_to_string(v);
        } else if (dict_get(cfg, "metadata.broker.list", v) && v) {
            kcfg.bootstrap_servers = k_to_string(v);
        }

        if (dict_get(cfg, "group.id", v) && v) {
            kcfg.group_id = k_to_string(v);
        }
        if (dict_get(cfg, "auto.offset.reset", v) && v) {
            kcfg.auto_offset_reset = k_to_string(v);
        }
        if (dict_get(cfg, "enable.auto.commit", v) && v) {
            if (v->t == -KB) kcfg.enable_auto_commit = (bool)v->g;
            else if (v->t == -KI) kcfg.enable_auto_commit = (v->i != 0);
            else if (v->t == -KJ) kcfg.enable_auto_commit = (v->j != 0);
            else kcfg.enable_auto_commit = (k_to_string(v) == "true");
        }

        // extra: everything else stringified
        K keys = kK(cfg)[0];
        K vals = kK(cfg)[1];
        if (keys && keys->t == KS) {
            for (J i = 0; i < keys->n; ++i) {
                std::string key = kS(keys)[i];
                if (key == "decode_threads" || key == "raw_queue_size" || key == "evt_queue_size" ||
                    key == "bootstrap.servers" || key == "metadata.broker.list" ||
                    key == "group.id" || key == "auto.offset.reset" || key == "enable.auto.commit")
                    continue;
                kcfg.extra[key] = k_to_string(kK(vals)[i]);
            }
        }
    }

    // sd1 callback: q main thread calls this when fd readable.
    // We only delegate to q function .kfkx.onfd[handle].
    static K kfkx_sd1_cb(I fd) {
        int handle = -1;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            auto it = g_fd2handle.find((int)fd);
            if (it != g_fd2handle.end()) handle = it->second;
        }

        if (handle > 0) {
            K r = k(0, (S)".kfkx.onfd", kj((J)handle), (K)0);
            if (r) r0(r);
        }
        return (K)0;
    }

} // namespace

/* ============================================================
 * ======================  C ABI  =============================
 * ============================================================ */
extern "C" {

    // kfkx_init(cfgDict) -> handle (long)
    K kfkx_initconsumer(K cfg) {
        kafkax::Core::DecodeConfig dcfg{};
        kafkax::Core::KafkaConfig  kcfg{};
        parse_cfg(cfg, dcfg, kcfg);

        std::string err;
        std::unique_ptr<kafkax::Core> core;

        try {
            core = std::make_unique<kafkax::Core>(dcfg, kcfg);
        } catch (const std::exception& e) {
            return krr((S)e.what());
        } catch (...) {
            return krr((S)"Core() failed");
        }

        int handle = g_next_handle.fetch_add(1);

        {
            std::lock_guard<std::mutex> lk(g_mu);
            Entry e;
            e.core = std::move(core);
            g_entries.emplace(handle, std::move(e));
        }

        return kj((J)handle);
    }

    // kfkx_close(handle) -> 1
    K kfkx_close(K h) {
        int handle = get_handle(h);
        if (handle <= 0) return krr((S)"bad handle");

        Entry ent;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            auto it = g_entries.find(handle);
            if (it == g_entries.end()) return krr((S)"unknown handle");

            if (it->second.sd1_registered && it->second.efd >= 0) {
                sd0(it->second.efd);
                g_fd2handle.erase(it->second.efd);
            }

            ent = std::move(it->second);
            g_entries.erase(it);
        }

        ent.core.reset();
        return ki(1);
    }

    // kfkx_subscribe(handle; topics) -> 1
    // topics: symbol atom or symbol list
    K kfkx_subscribe(K h, K topics) {
        int handle = get_handle(h);
        if (handle <= 0) return krr((S)"bad handle");

        std::vector<std::string> ts;
        if (k_is_sym_atom(topics)) ts.emplace_back(topics->s);
        else if (k_is_sym_vec(topics)) {
            ts.reserve((size_t)topics->n);
            for (J i=0;i<topics->n;++i) ts.emplace_back(kS(topics)[i]);
        } else return krr((S)"topics must be symbol atom or symbol list");

        Entry* e = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            auto it = g_entries.find(handle);
            if (it == g_entries.end()) return krr((S)"unknown handle");
            e = &it->second;
        }

        std::string err;
        if (e->core->subscribe(ts, err) != 0) return krr((S)err.c_str());

        // after subscribe, Core::start() has created eventfd
        int efd = e->core->notify_fd();
        if (efd < 0) return krr((S)"notify_fd invalid (start not called?)");

        // register sd1 once
        {
            std::lock_guard<std::mutex> lk(g_mu);
            auto& ent = g_entries[handle];
            if (!ent.sd1_registered) {
                ent.efd = efd;
                g_fd2handle[efd] = handle;
                ent.sd1_registered = true;
                sd1(efd, kfkx_sd1_cb);
            }
        }

        return ki(1);
    }

    // kfkx_bind(handle; topic; so_path; symbol) -> 1
    K kfkx_bind(K h, K topic, K so_path, K symbol) {
        int handle = get_handle(h);
        if (handle <= 0) return krr((S)"bad handle");
        if (!k_is_sym_atom(topic))  return krr((S)"topic must be symbol atom");
        if (!(k_is_sym_atom(so_path) || k_is_char_vec(so_path))) return krr((S)"so_path must be symbol or char vector");
        if (!k_is_sym_atom(symbol)) return krr((S)"symbol must be symbol atom");

        kafkax::Core* core = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            auto it = g_entries.find(handle);
            if (it == g_entries.end()) return krr((S)"unknown handle");
            core = it->second.core.get();
        }

        std::string err;
        if (core->bind_topic(topic->s, k_to_string(so_path), symbol->s, err) != 0)
            return krr((S)err.c_str());
        return ki(1);
    }

    // kfkx_rebind(handle; topic; so_path; symbol) -> 1
    K kfkx_rebind(K h, K topic, K so_path, K symbol) {
        int handle = get_handle(h);
        if (handle <= 0) return krr((S)"bad handle");
        if (!k_is_sym_atom(topic))  return krr((S)"topic must be symbol atom");
        if (!(k_is_sym_atom(so_path) || k_is_char_vec(so_path)))
            return krr((S)"so_path must be symbol or char vector");
        if (!k_is_sym_atom(symbol)) return krr((S)"symbol must be symbol atom");

        kafkax::Core* core = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            auto it = g_entries.find(handle);
            if (it == g_entries.end()) return krr((S)"unknown handle");
            core = it->second.core.get();
        }

        std::string err;
        std::string so = k_to_string(so_path);

        if (core->rebind_topic(topic->s, so, symbol->s, err) != 0)
            return krr((S)err.c_str());

        return ki(1);
    }

    // kfkx_unbind(handle; topic) -> 1
    K kfkx_unbind(K h, K topic) {
        int handle = get_handle(h);
        if (handle <= 0) return krr((S)"bad handle");
        if (!k_is_sym_atom(topic))  return krr((S)"topic must be symbol atom");

        kafkax::Core* core = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            auto it = g_entries.find(handle);
            if (it == g_entries.end()) return krr((S)"unknown handle");
            core = it->second.core.get();
        }

        // unbind has no err in your Core; treat non-zero as generic failure if it returns int
        int rc = core->unbind_topic(topic->s);
        if (rc != 0) return krr((S)"unbind_topic failed");

        return ki(1);
    }

    // kfkx_drain(handle; limit) -> table: tbl topic kind data err
    K kfkx_drain(K h, K limitK) {
        int handle = get_handle(h);
        if (handle <= 0) return krr((S)"bad handle");

        kafkax::Core* core = nullptr;
        int efd = -1;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            auto it = g_entries.find(handle);
            if (it == g_entries.end()) return krr((S)"unknown handle");
            core = it->second.core.get();
            efd  = it->second.efd;
        }

        std::size_t limit = 4096;
        if (limitK) {
            if (limitK->t == -KI) limit = (std::size_t)std::max(1, limitK->i);
            else if (limitK->t == -KJ) limit = (std::size_t)std::max<J>(1, limitK->j);
        }

        if (efd >= 0) drain_eventfd(efd);

        std::vector<kafkax::Event> evs;
        evs.reserve(limit);
        core->drainTo(evs, limit);

        J n = (J)evs.size();

        K col_tbl   = ktn(KS, n);
        K col_topic = ktn(KS, n);
        K col_kind  = ktn(KS, n);
        K col_data  = ktn(0,  n);
        K col_err   = ktn(0,  n);

        for (J i = 0; i < n; ++i) {
            const auto& ev = evs[(size_t)i];

            kS(col_topic)[i] = ss((S)ev.topic.c_str());

            kafkax::DecoderRegistry::BindingInfo bi{};
            if (core->get_topic_decoder(ev.topic, bi)) {
                // adjust if your field name differs
                kS(col_tbl)[i] = ss((S)bi.symbol.c_str());
            } else {
                kS(col_tbl)[i] = ss((S)ev.topic.c_str());
            }

            if (ev.kind == kafkax::Event::Kind::Error) {
                kS(col_kind)[i] = ss((S)"error");
                kK(col_data)[i] = ktn(KG, 0);
                // safe even if err_msg not null-terminated
                kK(col_err)[i]  = k_errvec(ev.err_msg, sizeof(ev.err_msg));
            } else {
                kS(col_kind)[i] = ss((S)"data");
                K b = ktn(KG, (J)ev.bytes.size());
                if (!ev.bytes.empty()) std::memcpy(kG(b), ev.bytes.data(), ev.bytes.size());
                kK(col_data)[i] = b;
                kK(col_err)[i]  = ktn(KC, 0);
            }
        }

        K names = ktn(KS, 5);
        kS(names)[0] = ss((S)"tbl");
        kS(names)[1] = ss((S)"topic");
        kS(names)[2] = ss((S)"kind");
        kS(names)[3] = ss((S)"data");
        kS(names)[4] = ss((S)"err");

        return xT(xD(names, knk(5, col_tbl, col_topic, col_kind, col_data, col_err)));
    }

} // extern C