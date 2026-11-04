# Routing host tests

Runs the real CDP routing code on your computer, with a fake radio and clock in place of the hardware. Each run builds with the address and undefined-behaviour sanitizers, once with remote commands off and once with them on.

## Run

The tests reuse the ArduinoJson and CRC32 sources that PlatformIO installs, so build any example once first:

```sh
EXAMPLE_DIR=Basic-Ducks/MamaDuck pio run -e local_lilygo_t_beam_sx1262
bash tests/routing-host/run.sh
```

To use other copies of those libraries, set `ARDUINOJSON_SRC` and `CRC32_SRC` to their `src` folders. Set `CXX` to use a compiler other than `clang++`. The output ends with `PASS`.

## What's covered

- **Route table:** a measured link isn't replaced by an indirect one, routes expire after `CDPCFG_ROUTE_TTL_MS`, and expiry survives the `millis()` rollover.
- **Signal health:** 0, 1, 2, 3, 6, 7 and 40 neighbours. Each payload fits in 228 bytes and reports truncation.
- **Malformed input:** bad route JSON, duplicate path entries, payloads over 228 bytes and hop counts over the limit are rejected.
- **Four-duck complete mesh:** 10 quiet minutes stay within 6 route requests and 18 replies. Every diagnostics page fits in 228 bytes even at maximum counter values. The PapaDuck receives repeated copies, and a full TX queue reports failure.
- **Three-link chain:** discovery reaches the PapaDuck, and replies keep their identity across hops. The duck recovers after losing the gateway, and after a relay reboots.
- **Startup pacing:** sends are queued and paced, then delivered.
- **5.0 compatibility:** PapaDuck data uses the `00000000` header address, route JSON uses `"11111111"`, and packets addressed to `11111111` are accepted as PapaDuck traffic.

These tests don't model RF loss, collisions, power or real timing. Hardware testing is still needed before a release.
