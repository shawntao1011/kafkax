# kafkax – Kafka Decode Extension Framework

`kafkax` is a lightweight Kafka consumer + decoder bridge designed for kdb+/q environments.

It focuses on:

- Stable C ABI surface
- Plugin-based decoder model
- Clean integration with q via a single shared object
- Predictable performance characteristics

---

## Build Output

The current build produces a single shared library: ```libkafkax_q.so```

This is the only required artifact for q-side integration.

---

## Custom Decoder Plugin ABI

Decoder plugins must follow the ABI defined in: 

```include/kafkax/decoder.h```

That header is the single source of truth for:

- ABI versioning
- Required exported symbols
- Input/output buffer contract
- Error handling semantics

The README does **not** duplicate ABI rules.  
Always consult the header file.

## Current Status

- Basic Kafka consume loop
- Decoder plugin loading
- q IPC table encoder (qipc)
- Internal buffering and dispatch

This is a pilot-stage release intended for integration testing.

---

# Roadmap

## 1. Kafka Observability (C Layer)

Add native support for:

- `stats_cb`
- `error_cb`

Goals:

- Expose Kafka runtime metrics
- Adapt `stats_cb` output to Prometheus format
- Provide stable metrics surface for monitoring
- Improve production observability

---

## 2. Performance Optimization

Primary focus:

- Optimize qipc encoding path
- Reduce dynamic allocations
- Minimize memory copies
- Improve large payload throughput

Planned direction (high-level):

- Writer-based encoding model
- Encode-into-buffer API (eliminate intermediate vectors)
- Further reduction of allocator pressure

---

## 3. API Stabilization

- Harden decoder ABI boundary
- Improve error propagation model
- Clarify pause/resume semantics
- Introduce operational controls (pause/drain/seek)

---

# Versioning

`v1.0.0` is a pilot test release.

The ABI and performance model may evolve in future major versions.

# License

Apache License 2.0.

---