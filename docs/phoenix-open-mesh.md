# Phoenix open mesh: FIR-23, FIR-15 and FIR-8

Candidate implementation, 22 October 2026. Hardware qualification is still required.

## Behaviour and wire contract

Successful radio setup makes a node locally ready to send and receive. `PUBLIC`
means that local readiness; it does not mean a gateway or cloud connection was
verified. There is no network-membership handshake. Compatible radio settings
are still required. Radio frequency, power, bandwidth and sync word are unchanged.

Application sends return success only after enqueueing into the bounded TX queue.
No known gateway route is required. Relaying nodes flood application packets with
the existing duplicate filter and a maximum of six relays. Queue acceptance is
not a receipt acknowledgement. There is no persistent offline store; a full queue
returns `DUCK_ERR_QUEUE_FULL`, a failed radio start is counted, and a reboot or
sleep can still discard pending packets. Papa's application callback continues to
receive repeated telemetry copies for the intentional MQTT duplicate policy.

The 30-minute route-entry TTL and 10-minute keepalive are retained. Each non-Papa
participating node sends one startup neighbour discovery after a randomized
2–6 seconds, then a one-hop broadcast discovery every ten minutes. If the gateway
route is absent, application sends may initiate a targeted discovery, globally
limited to once per 30 seconds per origin. Keepalive also attempts gateway
discovery if necessary. Discovery reasons are startup (1), missing destination
(2), neighbour keepalive (3) and missing gateway at keepalive (4).

The 27-byte packet header is unchanged. Control JSON uses these rules:

- An RREQ's header destination and JSON destination identify the final target.
  Broadcast is represented by `"*"` in JSON, avoiding invalid UTF-8 0xFF bytes.
  Origin, MUID and hop count survive forwarding. The path starts with the origin
  and grows by one actual transmitter per relay. A node already in the path does
  not forward it. Broadcast neighbour requests are not relayed.
- A new RREP has one new MUID at its actual responder. Its JSON `origin` is the
  discovered destination, `destination` is the requester, and `path` is the
  request's reverse stack. `q` is the original request MUID as eight hex digits.
  `via` is the actual immediate transmitter. The header destination is the next
  receiver from that stack, rather than the final requester.
- Only that addressed receiver forwards an RREP. It pops itself, updates `via`,
  increments hops and preserves responder SDUID, MUID, duck type and `q`.
  Overhearing the reply never authorizes regenerating it. A finite path and hop
  limit prevent loops independently of the bounded 64-entry exact control cache.
- `11111111` is a gateway destination alias. Direct neighbour observations use
  the gateway's actual DUID. An indirect route never replaces a measured link.
  Routes retain three independently expiring candidates per destination;
  `getBestNextHop()` returns the selected candidate's actual next hop.
- Link nodes remain leaves; detectors remain observers. Neither advertises a
  transit route. Control and data queues alternate when both contain work, with
  the existing three-second spacing. A busy radio leaves the queued item pending.

Update all four bench nodes together. Legacy direct replies can be consumed,
but mixed-version multi-hop control is not qualified. Older nodes can still
regenerate looping replies. Do not infer fleet readiness from one upgraded node.

## Signal health

The existing `s` sender and `n` array of `[actualDuid, normalizedRssi, normalizedSnr]`
remain compatible with DMS. Values are normalized using the existing -131..-20 dBm
and -30..30 dB ranges, rounded to four decimal places. RX signal/time are captured
before queueing, rather than sampled later from an unrelated received packet.

Additional fields are `v:2`, `o` fresh directly observed identities retained,
`i` included identities, `tr` truncation and `a` oldest included observation age
in seconds (`null` if empty). Inclusion uses actual serialized size, maximum 228
bytes, and a six-peer ceiling. Zero peers is an explicit empty array. Storage
retains at most 32 direct peers and 64 route destinations; direct-peer capacity
evictions are counted separately. All expiry comparisons tolerate timer rollover.

A first signal snapshot is attempted with the first five-minute diagnostic page,
then the original 195-minute signal timer remains active. This adds one topology
packet per boot to permit a short installation preflight. The existing DMS
formatter maps every array entry, but its formatted signal display/CSV omits the
new metadata. Raw MQTT payloads and serial diagnostic page 3 preserve completeness evidence;
the displayed neighbour array still needs an actual three-peer cloud check.

## Simple radio health v3

Every five minutes the node enqueues one flat status message, for example:

```json
{"v":3,"C":"12","FM":"291340","boot":2488545892,"uptime_s":3604,"neighbors":3,"tx_started":79,"tx_done":79,"tx_failed":0,"tx_timeouts":0,"queue_drops":0}
```

| Field | Meaning |
| --- | --- |
| `v` | Health payload schema version, now 3. The CDP packet header is unchanged. |
| `C` / `FM` | Health enqueue counter and free heap in bytes; legacy string fields retained. `C` advances after a health message is accepted into the queue. |
| `boot` / `uptime_s` | Random boot identifier and seconds since boot. Compare counters only within the same boot. Uptime derived from `millis()` wraps after approximately 49.7 days. |
| `neighbors` | Current fresh directly observed neighbor count. |
| `tx_started` / `tx_done` | Accepted radio starts and physical transmission completions, including routing traffic. Completion is not a Papa receipt or cloud acknowledgement. One transmission can still be in progress when sampled. |
| `tx_failed` / `tx_timeouts` | Failed radio starts and physical transmission timeouts. Normal busy waits are excluded. |
| `queue_drops` | Sum of RX, application-TX and control-TX rejected enqueues. The sum uses 64 bits so adding the three 32-bit counters cannot overflow. |

The original 132-minute health timer emits the same summary. The five-minute
cadence, routing behavior, packet header and 228-byte data limit are unchanged.
The summary contains no positional arrays. Radio counters reset at boot and
wrap modulo 2^32; each individual queue-rejection counter also wraps modulo
2^32 before aggregation. Payload size is checked before enqueueing.

Papa and generic JSON consumers can forward this payload unchanged. Consumers
that specifically decode v2 health pages must recognize v3 rather than assume
that a missing array means a zero counter. Signal-health payloads keep their
existing separate schema. Already-flashed v2 firmware continues to emit v2;
updating this source branch does not update devices pinned to an older commit.

## Detailed serial diagnostics v2

Full diagnostic counters remain available through `routingDiagnostics(page)`.
At each five-minute heartbeat, one v2 page is logged locally as
`[CDP_DIAG] health_detail={...}`; four successful heartbeats form a 20-minute
rotation. These pages are no longer automatically transmitted over the radio.
Headless/cloud-only deployments therefore receive the simple summary; detailed
queue, routing, duplicate and signal-snapshot inspection requires serial capture
or an explicit application call to `routingDiagnostics(page)`.

Counters are cumulative unsigned 32-bit values, reset at boot and interpreted
modulo 2^32. Every page has
`v`, page `p`, random per-boot epoch `b`, and seconds `u` from `millis()` (which
wraps after approximately 49.7 days). Compare by node + epoch, never across boots.

| Page | Fields and semantics |
| --- | --- |
| 0 | Legacy string `C` / `FM`, reset reason `r`, state `n` (0 searching, 1 locally ready, 2 disconnected), selected gateway next hop `gw` and route age `ga` in seconds, fresh direct count `nb`, discovery attempts `j`, last reason `jr`, state transitions `nt`. `gw`/`ga` are null without a cached route; Papa reports `self` / 0. A cached route is not a live reachability probe. |
| 1 | Arrays `q` current queue depths, `h` high-water marks, `d` rejected enqueues, each ordered RX / application-TX / control-TX. Full queues reject the newest item. |
| 2 | `rq` RREQ and `rp` RREP arrays ordered originated-and-enqueued / forwarded-and-enqueued / duplicate-suppressed / dropped. `dr` is malformed controls / hop-limit drops (including data) / rejected loop or return path. Overheard replies addressed elsewhere are excluded, not counted as errors. Radio CRC failures occur before this layer. |
| 3 | `tx` accepted radio starts / physical TX_DONE IRQs / failed radio starts / busy deferrals / physical TX timeouts. `app` originated application enqueues / relay enqueues / suppressed data duplicates. `sg` last signal snapshot observed / included / seconds since serialization, or null before one exists. `ne` direct-peer capacity evictions. |

Missing pages or null snapshots are unavailable evidence, never healthy zeroes.
Pages are individually bounded; do not merge values from different epochs.
`[CDP_DIAG] TX event=done` is emitted at the actual IRQ and is recognized by Loggy.
`TX data done in` still means start acceptance for backwards log compatibility.
The radio recovers to RX after a missed-TX-IRQ deadline based on time on air plus
two seconds. ISR SPI servicing and the earlier CRC/error fixes are retained.

Set `CDPCFG_LOG_PACKET_HEX` in the bench profile to expose exact serialized packet
identities at INFO level for Loggy's unique origin-to-Papa-to-cloud comparison.
Normal builds can leave it disabled. `[CDP_DIAG] ACCEPT` separately identifies
successful application queue acceptance. Receipt at Papa and cloud storage must
still be measured from their respective evidence; neither is inferred by CDP.

## Software evidence and bench limits

The regression harness lives in phoenix-zoo's deployment `tests/routing-host`,
outside the CDP core. It compiles these actual source files with a fake radio and
clock, plus address/undefined-behaviour sanitizers. It does not simulate RF loss,
collisions, power, oscillator drift or MQTT.

In the identical four-node complete-graph ten-minute quiet comparison, baseline
`e8075ff2` generated 3 RREQ and 153 RREP transmissions. The candidate generated
3 RREQ and 9 RREP: one request from each of three nodes, three neighbours each
replying once. This is the nominal cost of a healthy all-direct bench, 72 control
transmissions per awake hour across the four nodes, excluding startup/recovery.
Multi-hop targeted discovery has a different bounded cost and is tested separately.

For the next all-direct bench, after a two-minute settling period, require no
sustained RREP circulation, at most six originated keepalives and eighteen replies
in any ten-minute observation crossing a cadence boundary, no queue rejects or
radio failures/timeouts during the undisturbed interval, and three fresh directly
observed neighbours reported through cloud/display for each non-Papa. A controlled
three-link discovery or reboot should converge within 90 seconds on an unloaded
bench; a quiet late gateway should recover within one keepalive period plus 90
seconds. These are qualification limits, not claimed hardware results.

Log interruptions and deliberate faults separately. Then score a complete 24-hour
uninterrupted run, including scheduled sleep/wake, cold/wake boot JSON, 23-feature
integrity, unique application acceptance-to-cloud delivery and every missing
identity. USB capture does not prove no-USB boot. The earlier ML disappearance,
PMU failures and early wakes remain separate unresolved findings under FIR-21.

The radio limit is 255 bytes total, so payloads are bounded to 228 bytes after
the 27-byte header. The Teensy formatter uses the same bound and retries compact
formatting while preserving all 23 features. A 229-byte body is rejected before
radio submission; the previous 256-byte total could never fit the driver limit.
