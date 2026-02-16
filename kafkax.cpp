
#include "kafkax/core.hpp"
#include "kx/k.h"

#define KNL (K) 0
#ifdef __GNUC__
#  define UNUSED(x) x __attribute__((__unused__))
#else
#  define UNUSED(x) x
#endif

/* ============================================================
 * ======================  Global =============================
 * ============================================================ */

std::mutex g_mu;
std::unique_ptr<kafkax::Core> g_core;

int g_fd[2] = {-1, -1};
std::atomic<bool> g_inited{false};

/* ---------------------------------------------------------
   helper
--------------------------------------------------------- */
static void set_err(char* dst, std::size_t cap, const char* s) {
    if (!dst || cap == 0) return;
    if (!s) { dst[0] = '\0'; return; }
    std::snprintf(dst, cap, "%s", s);
    dst[cap - 1] = '\0';
}


/* ============================================================
 * ======================  C ABI  =============================
 * ============================================================ */
extern "C" {

    K kafkax_callback(I) {

    }

    K kafkax_init(K UNUSED(x)) {
        std::lock_guard<std::mutex> lk(g_mu);

        if (g_inited.load())
            return KNL;

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, g_fd) != 0)
            return krr((S)"socketpair failed");

        sd1(-g_fd[0], kafkax_callback);

        g_inited.store(true);

        return KNL;
    }

    K kafkax_consumer(K dict) {

    }

    // specify decoder function
    K kafkax_subscribe(K topic, K libpath, K symbol) {

    }

    K kafkax_rebind(K topic, K libpath, K symbol) {

    }

} // extern C