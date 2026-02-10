# kafkax – Kafka Decode Extension Framework

`kafkax` is a lightweight framework for extending Kafka message processing with
**dynamically loaded decode functions**.

It defines a stable C ABI for decoder plugins and a minimal runtime to bind
Kafka topics to user-provided decode logic.

> `subscribe(topic)` is sugar for  
> `subscribe(topic, default_decoder)`.

---

## Build

### Requirements

- CMake ≥ 3.16
- C/C++ compiler with C++20 support
- POSIX dynamic loader (Linux / macOS)

### Build steps

```bash
git clone <repo>
cd kafkax
cmake -S . -B build
cmake --build build
```

Artifacts:

- `libkafkax.so`
- `libkafkax_default_decoder.so`
- `subscribe_demo`

---

## Usage

### Default decoder

```c
kafkax_subscribe("marketdata.demo");
```

Equivalent to:

```c
kafkax_subscribe_with_decoder(
    "marketdata.demo",
    "libkafkax_default_decoder.so",
    "kafkax_default_decode"
);
```

The default decoder performs a raw passthrough of the Kafka payload.

---

### Custom decoder plugin

Decoders must implement the ABI defined in:

```
include/kafkax/decoder.h
```

Minimal decoder example:

```c
#include "kafkax/decoder.h"

int kafkax_decoder_abi_version() {
    return KAFKAX_DECODER_ABI_VERSION;
}

int my_decode(
    const kafkax_envelope_t* env,
    kafkax_decode_result_t* out
) {
    out->kind = 0;
    out->msg_type = 0;
    out->out_bytes = env->value;
    out->out_len = env->value_len;
    return 0;
}
```

Usage:

```c
kafkax_subscribe_with_decoder(
    "marketdata.custom",
    "/path/to/libmydecoder.so",
    "my_decode"
);
```

---

## Decoder Contract (Summary)

- C ABI, versioned
- Loaded via `dlopen` / `dlsym`
- Input: immutable Kafka envelope
- Output: decoded payload or error
- All decoders (including built-in) follow the same contract

---

## License

Apache License 2.0.

---