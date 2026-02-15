#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================
 *  Opaque handle
 * ================================ */

typedef void* kafkax_handle;


/* ================================
 *  Lifecycle
 * ================================ */

/* Create core instance (no Kafka started yet). */
kafkax_handle
kafkax_create(int32_t decode_threads,
              char* err_buf,
              int32_t err_cap);

/* Destroy core and free all resources. */
void
kafkax_destroy(kafkax_handle h);

/* Start internal Kafka consumer + decode workers. */
int
kafkax_start(kafkax_handle h,
             char* err_buf,
             int32_t err_cap);

/* Stop internal threads (idempotent). */
void
kafkax_stop(kafkax_handle h);


/* ================================
 *  Kafka Configuration
 * ================================ */

/* Generic Kafka configuration (must be called before start). */
int
kafkax_set_conf(kafkax_handle h,
                const char* key,
                const char* value,
                char* err_buf,
                int32_t err_cap);


/* ================================
 *  Topic Subscription + Decoder
 * ================================ */

/* Subscribe topic and bind decoder.
 *
 * If topic already exists → error.
 *
 * decoder_lib == NULL → use builtin decoder
 */
int
kafkax_subscribe(kafkax_handle h,
                 const char* topic,
                 const char* decoder_lib,
                 const char* decoder_fn,
                 char* err_buf,
                 int32_t err_cap);


/* Rebind decoder for an existing topic.
 *
 * Does NOT re-subscribe Kafka.
 * Only changes decoder used by decode workers.
 *
 * Safe to call while running.
 */
int
kafkax_rebind_decoder(kafkax_handle h,
                      const char* topic,
                      const char* decoder_lib,
                      const char* decoder_fn,
                      char* err_buf,
                      int32_t err_cap);


/* Unsubscribe topic (removes decoder binding too). */
int
kafkax_unsubscribe(kafkax_handle h,
                   const char* topic,
                   char* err_buf,
                   int32_t err_cap);


/* Get current decoder binding for topic.
 *
 * out_lib / out_fn are owned by core (do NOT free).
 */
int
kafkax_get_decoder(kafkax_handle h,
                   const char* topic,
                   const char** out_lib,
                   const char** out_fn);


/* ================================
 *  Drain (Pull Model)
 * ================================ */

typedef enum {
    KAFKAX_EVENT_DATA  = 0,
    KAFKAX_EVENT_ERROR = 1
} kafkax_event_kind;

typedef struct {

    kafkax_event_kind kind;

    const char* topic;

    const uint8_t* key;
    int32_t key_len;

    int64_t ingest_ns;

    /* DATA only */
    const uint8_t* data;
    int32_t data_len;

    /* ERROR only */
    const char* error_msg;

} kafkax_event_view;


typedef void (*kafkax_drain_cb)(const kafkax_event_view* ev,
                                void* user);

/* Drain decoded events (pull model).
 *
 * max_events <= 0 means drain all available.
 *
 * Returns number drained.
 */
int32_t
kafkax_drain(kafkax_handle h,
             int32_t max_events,
             kafkax_drain_cb cb,
             void* user);


/* ================================
 *  Introspection
 * ================================ */

/* Number of decoded events waiting in queue. */
int32_t
kafkax_pending_events(kafkax_handle h);

/* Number of subscribed topics. */
int32_t
kafkax_topic_count(kafkax_handle h);

#ifdef __cplusplus
}
#endif
