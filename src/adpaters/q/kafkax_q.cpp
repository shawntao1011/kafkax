#include "kafkax/api.h"

#include <cstring>
#include <string>
#include <vector>

#include "core/core.hpp"
#include "kafkax/envelope.h"

namespace {

static void set_err(char* dst, int32_t cap, const char* s) {
    if (!dst || cap <= 0) return;
    if (!s) { dst[0] = '\0'; return; }
    std::snprintf(dst, (size_t)cap, "%s", s);
    dst[cap - 1] = '\0';
}

struct Handle {
    kafkax::Core core;
    explicit Handle(int32_t threads)
        : core(kafkax::Core::Config{ (std::size_t)(threads > 0 ? threads : 1), 8192, 8192 }) {}
};

static constexpr const char* kDefaultLib = "libkafkax_default_decoder.so";
static constexpr const char* kDefaultFn  = "kafkax_default_decode";

kafkax_handle kafkax_create(int32_t decode_threads) {
    try {
        return (kafkax_handle)(new Handle(decode_threads));
    } catch (...) {
        return nullptr;
    }
}

void kafkax_destroy(kafkax_handle h) {
    if (!h) return;
    delete (Handle*)h;
}

int kafkax_subscribe(kafkax_handle h,
                     const char* topic,
                     const char* decoder_lib,
                     const char* decoder_fn,
                     char* err_buf,
                     int32_t err_cap) {
    if (!h || !topic || !decoder_lib || !decoder_fn) {
        set_err(err_buf, err_cap, "invalid args");
        return -1;
    }

    try {
        kafkax::DecoderBinding b;
        b.libpath = decoder_lib;
        b.fnname  = decoder_fn;
        ((Handle*)h)->core.bind_topic(topic, std::move(b));
        return 0;
    } catch (...) {
        set_err(err_buf, err_cap, "subscribe failed");
        return -1;
    }
}

int kafkax_subscribe_default(kafkax_handle h,
                             const char* topic,
                             char* err_buf,
                             int32_t err_cap) {
    return kafkax_subscribe(h, topic, kDefaultLib, kDefaultFn, err_buf, err_cap);
}

int kafkax_push(kafkax_handle h,
                const char* topic,
                const uint8_t* key, int32_t key_len,
                const uint8_t* payload, int32_t payload_len,
                int64_t ingest_ns) {
    if (!h || !topic) return -1;

    kafkax::Envelope env;
    env.topic = topic;

    if (key && key_len > 0) env.key.assign(key, key + key_len);
    if (payload && payload_len > 0) env.payload.assign(payload, payload + payload_len);
    env.ingest_ns = ingest_ns;

    ((Handle*)h)->core.push(std::move(env));
    return 0;
}

int32_t kafkax_drain(kafkax_handle h, int32_t max_events, kafkax_event_cb cb, void* user) {
    if (!h || !cb || max_events <= 0) return 0;

    std::vector<kafkax::Event> out;
    out.reserve((size_t)max_events);

    ((Handle*)h)->core.drain(out, (size_t)max_events);

    int32_t n = 0;
    for (auto& e : out) {
        kafkax_event_view_t v{};
        v.kind = (e.kind == kafkax::Event::Kind::Data) ? 0 : 1;
        v.topic = e.topic.c_str();
        v.key = e.key.empty() ? nullptr : e.key.data();
        v.key_len = (int32_t)e.key.size();
        v.ingest_ns = e.ingest_ns;
        v.decoder = e.decoder.c_str();
        v.bytes = e.bytes.empty() ? nullptr : e.bytes.data();
        v.bytes_len = (int32_t)e.bytes.size();
        v.err_msg = e.err_msg;

        cb(&v, user);
        ++n;
    }

    return n;
}

} // namespace

extern "C" {

    // Global handle for demo. Production should manage lifecycle explicitly.
    static kafkax_handle g_h = nullptr;

    static void noop_consume(const kafkax_event_view_t* /*ev*/, void* /*user*/) {}

    extern "C" K kafkax_init(K threads) {
        if (threads->t != -7) return krr((S)"type");
        if (g_h) return (K)0;
        g_h = kafkax_create((int32_t)threads->j);
        return g_h ? (K)0 : krr((S)"init");
    }

    extern "C" K kafkax_close() {
        if (g_h) {
            kafkax_destroy(g_h);
            g_h = nullptr;
        }
        return (K)0;
    }

    // subscribe_default(topic)
    extern "C" K kafkax_subscribe(K topic) {
        if (!g_h) return krr((S)"init");
        if (topic->t != -11) return krr((S)"type");
        char err[128]{0};
        int rc = kafkax_subscribe_default(g_h, topic->s, err, (int32_t)sizeof(err));
        return rc == 0 ? (K)0 : krr(err[0] ? (S)err : (S)"subscribe");
    }

    // poll(n): currently just drains and discards, returns count.
    // Later you can return list-of-dicts to q.
    extern "C" K kafkax_poll(K n) {
        if (!g_h) return krr((S)"init");
        if (n->t != -7) return krr((S)"type");
        int32_t maxn = (int32_t)n->j;
        int32_t got = kafkax_drain(g_h, maxn, &noop_consume, nullptr);
        return kj(got);
    }

} // extern "C"
