"""Minimal MIDI reader. Prints tracks, tempo, and notes (start, length, pitch) per channel.
Args: file.mid"""
import struct
import sys
from collections import defaultdict

NAMES = ["C", "CS", "D", "DS", "E", "F", "FS", "G", "GS", "A", "AS", "B"]


def name(n):
    return f"{NAMES[n % 12]}{n // 12 - 1}"


def varlen(d, i):
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i


data = open(sys.argv[1], "rb").read()
assert data[:4] == b"MThd", "not a MIDI file"
hlen, fmt, ntrk, div = struct.unpack(">IHHH", data[4:14])
print(f"format {fmt}, tracks {ntrk}, ticks/quarter {div}")
pos = 8 + hlen

notes = []  # (track, channel, start, end, pitch, velocity)
tempos = []
for t in range(ntrk):
    assert data[pos:pos + 4] == b"MTrk"
    tlen = struct.unpack(">I", data[pos + 4:pos + 8])[0]
    d = data[pos + 8:pos + 8 + tlen]
    pos += 8 + tlen
    i = 0
    tick = 0
    status = 0
    on = {}
    tname = ""
    while i < len(d):
        dt, i = varlen(d, i)
        tick += dt
        b = d[i]
        if b & 0x80:
            status = b
            i += 1
        if status == 0xFF:
            mtype = d[i]
            ln, i = varlen(d, i + 1)
            body = d[i:i + ln]
            i += ln
            if mtype == 0x51:
                tempos.append((tick, int.from_bytes(body, "big")))
            elif mtype == 0x03:
                tname = body.decode("latin1")
            elif mtype == 0x58:
                print(f"track {t}: time signature {body[0]}/{2 ** body[1]}")
        elif status in (0xF0, 0xF7):
            ln, i = varlen(d, i)
            i += ln
        else:
            kind = status & 0xF0
            ch = status & 0x0F
            if kind in (0xC0, 0xD0):
                a = d[i]
                i += 1
                if kind == 0xC0:
                    print(f"track {t} ch {ch}: program {a}")
                continue
            a, v = d[i], d[i + 1]
            i += 2
            if kind == 0x90 and v > 0:
                on[(ch, a)] = (tick, v)
            elif kind in (0x80, 0x90):
                if (ch, a) in on:
                    s, vel = on.pop((ch, a))
                    notes.append((t, ch, s, tick, a, vel))
    print(f"track {t}: name '{tname}', end tick {tick}")

print("tempos (tick, bpm):", [(tk, round(60e6 / us, 2)) for tk, us in tempos])
groups = defaultdict(list)
for n in notes:
    groups[(n[0], n[1])].append(n)
for (t, ch), ns in sorted(groups.items()):
    ns.sort(key=lambda n: (n[2], -n[4]))
    pitches = [n[4] for n in ns]
    print(f"\n== track {t} ch {ch}: {len(ns)} notes, range {name(min(pitches))}-{name(max(pitches))}")
    for n in ns[: int(sys.argv[2]) if len(sys.argv) > 2 else 60]:
        print(f"  start {n[2] / div:7.3f}q  len {(n[3] - n[2]) / div:6.3f}q  {name(n[4]):4} ({n[4]}) vel {n[5]}")
