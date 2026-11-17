"""MIDI -> song.h in the chiptune format: {NOTE_x, divider} pairs per voice, REST for gaps.
Divider like the presented code: 16/divider sixteenths, negative = dotted (24/-divider).
melody = top note (>= C4), harmony = second note (>= C4), bass = lowest note (< C4).
Args: in.mid out.h"""
import contextlib
import os
import io
import runpy
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
src, out = sys.argv[1], sys.argv[2]
sys.argv = ["x", src, "0"]
with contextlib.redirect_stdout(io.StringIO()):
    g = runpy.run_path(os.path.join(HERE, "midi_dump.py"))
notes, div, tempos, name = g["notes"], g["div"], g["tempos"], g["name"]
S16 = div // 4
bpm = round(60e6 / tempos[0][1]) if tempos else 120
bars = -(-round(max(n[3] for n in notes) / S16) // 16)
total = bars * 16

# length in sixteenths -> divider, biggest first
DIVIDERS = [(16, 1), (12, -2), (8, 2), (6, -4), (4, 4), (3, -8), (2, 8), (1, 16)]


def split(length):
    parts = []
    for l, d in DIVIDERS:
        while length >= l:
            parts.append((l, d))
            length -= l
    return parts


def voice(ns, rank):
    by_start = {}
    for n in ns:
        s = round(n[2] / S16)
        by_start.setdefault(s, {})[n[4]] = max(s + 1, round(n[3] / S16))
    seq = []
    for s in sorted(by_start):
        pitches = sorted(by_start[s])
        if len(pitches) < abs(rank) + (rank >= 0):
            continue
        p = pitches[rank]
        seq.append((s, by_start[s][p], p))
    pairs, t = [], 0
    for i, (s, e, p) in enumerate(seq):
        if i + 1 < len(seq):
            e = min(e, seq[i + 1][0])
        e = min(e, total)
        pairs += [("REST", d) for _, d in split(s - t)]
        first, *rest = split(e - s)
        pairs.append((f"NOTE_{name(p)}", first[1]))
        pairs += [("REST", d) for _, d in rest]
        t = e
    pairs += [("REST", d) for _, d in split(total - t)]
    return pairs


def array(nm, pairs):
    lines = [f"const int {nm}[] PROGMEM = {{"]
    for i in range(0, len(pairs), 4):
        lines.append("  " + " ".join(f"{p + ',' + str(d) + ',':12}" for p, d in pairs[i:i + 4]).rstrip())
    lines.append("};")
    return "\n".join(lines)


treble = [n for n in notes if n[1] != 9 and n[4] >= 60]
low = [n for n in notes if n[1] != 9 and n[4] < 60]
melody, harmony, bass = voice(treble, -1), voice(treble, -2), voice(low, 0)

text = "\n".join([
    f"// Generated from {os.path.basename(src)} by tools/gen_chiptune.py, same format as the melody in chiptune.",
    f"#define BPM {bpm}",
    f"#define SONG_BARS {bars}",
    "",
    array("melody", melody), "",
    array("harmony", harmony), "",
    array("bass", bass), "",
])
open(out, "w", newline="\n").write(text)
print(f"bpm {bpm}, {bars} bars, pairs: melody {len(melody)}, harmony {len(harmony)}, bass {len(bass)}")
