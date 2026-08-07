# Health packet wire format

Health packets use the CDP `health` topic and contain compact JSON keys so the
payload fits within the CDP maximum packet size.

JSON application payloads sent through `sendData()` receive an `uptime` field
immediately before packet creation. The value is the sender's elapsed time
since boot in milliseconds. Every originating CDP packet also carries the
same value as wire metadata; relayed packets retain the original sender's
wire-level value.

| Key | Meaning | Lifetime |
| --- | --- | --- |
| `C` | Successful packets sent | Cumulative for the Duck's lifetime |
| `M` | Free memory | Point-in-time |
| `uptime` | Sender uptime in milliseconds | Point-in-time |
| `RT` | Received radio frames, including CRC failures | Since the last successfully sent health packet |
| `RV` | Valid received CDP packets | Since the last successfully sent health packet |
| `RC` | CRC failures at the radio or CDP layer | Since the last successfully sent health packet |
| `RI` | Received packets with invalid length | Since the last successfully sent health packet |
| `RR` | Radio packet read failures | Since the last successfully sent health packet |
| `TA` | Transmission attempts started | Since the last successfully sent health packet |
| `TS` | Transmissions completed successfully | Since the last successfully sent health packet |
| `TF` | Transmission failures | Since the last successfully sent health packet |
| `QD` | Valid packets dropped from the receive queue | Since the last successfully sent health packet |
| `QT` | Packets dropped from a transmit queue | Since the last successfully sent health packet |
| `D` | Duplicate packets detected | Since the last successfully sent health packet |
| `FD` | Packets successfully queued for forwarding | Since the last successfully sent health packet |
| `FF` | Forwarding failures | Since the last successfully sent health packet |
| `JA` | Network join attempts | Since the last successfully sent health packet |
| `JS` | Successful network joins | Since the last successfully sent health packet |
| `JF` | Failed join attempts or search fallbacks | Since the last successfully sent health packet |
| `RQS` | Route requests sent | Since the last successfully sent health packet |
| `RQR` | Route requests received | Since the last successfully sent health packet |
| `RPS` | Route responses sent | Since the last successfully sent health packet |
| `RPR` | Route responses received | Since the last successfully sent health packet |
| `RM` | Route lookup misses | Since the last successfully sent health packet |

Interval counters are reset only after the health packet receives a radio
`TX_DONE` event. If the health packet fails, the counters are retained for the
next report.
