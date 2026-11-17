Problema:
TODO

Dizainas:
TODO

Dalys:
## Components

| Name | Quantity | Component |
| ---- | -------- | --------- |
| U1 | 1 | Arduino Uno R3 |
| BZ1 | 1 | Pasyvus buzzeris |
| PZ1, PZ2 | 2 | Pjezo diskai |
| K1 | 1 | Relė Axicom D2n 5 V |
| Q1 | 1 | TIP120 |
| D1 | 1 | Diodas |
| R1, R2 | 2 | 1 kΩ |
| D2 | 1 | Raudonas LED |
| SERVO1 | 1 | Servo |
| S1 | 1 | Mygtukas |

Schema:
![[wiring.png]]
![[scheme.png]]

```
D2 ── 1 kΩ ── tranzistoriaus B; E ── GND; 5V ── relės ritė ── C; diodas per ritę (juostelė prie 5V)
D3 ── mygtukas ── GND
D4 ── 1 kΩ ── LED ── buzzeris ── GND
D8 ── servo signalas (5V, GND)
D9 ── diskas ── GND
D10 ── diskas ── GND
```

Kas veikia/neveikia.
TODO

Pagerinimas.
TODO
