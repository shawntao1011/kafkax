
#include "core/core.hpp"

/* ============================================================
 * ======================  Global =============================
 * ============================================================ */

/* ============================================================
 * ======================  C ABI  =============================
 * ============================================================ */
extern "C" {

    K kafkax_callback(I) {

    }

    K kafkax_init(K UNUSED(x)) {

    }

    K kafkax_conf(K dict) {

    }

    // This uses the default pass through decoder
    K kafkax_subscribe(K topic) {

    }

    // User Specified decoder
    K kafkax_subscribe(K topic, K libpath, K symbol) {

    }

    K kafkax_rebind(K topic, K libpath, K symbol) {

    }

} // extern C