"""Build Chisel's built-in sample bank: app/bank.h.

Eight slots. WAV files in samples/ (sorted by name, first eight) fill them;
any slot left empty gets a generated demo sound, so the repository never
needs to carry recordings. Samples are resampled to 32 kHz mono and stored
as Chisel's blocked IMA ADPCM (engine/ch_adpcm.h), which reads in place from
flash. A BPM in the file name ("loop 93bpm.wav") sets DISC's TEMPO.

    python -I tools/make_bank.py [--budget-kb 300]
"""
import math, os, random, re, struct, sys

RATE, BLOCK = 32000, 64
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

STEPS = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
         50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
         253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
         1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
         3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
         11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def step(code, pred, index):
    st = STEPS[index]
    diff = st >> 3
    if code & 4: diff += st
    if code & 2: diff += st >> 1
    if code & 1: diff += st >> 2
    pred = pred - diff if code & 8 else pred + diff
    pred = max(-32768, min(32767, pred))
    index = max(0, min(88, index + IDX[code]))
    return pred, index


def adpcm(pcm):
    """The same encoder as engine/ch_adpcm.c (greedy best code per step)."""
    out = bytearray()
    index = 0
    for start in range(0, len(pcm), BLOCK):
        pred = pcm[start]
        blk = bytearray(4 + BLOCK // 2)
        struct.pack_into('<hBB', blk, 0, pred, index, 0)
        for i in range(BLOCK):
            target = pcm[start + i] if start + i < len(pcm) else 0
            best = None
            for code in range(16):
                p, ix = step(code, pred, index)
                e = abs(p - target)
                if best is None or e < best[0]:
                    best = (e, code, p, ix)
            _, code, pred, index = best
            if i & 1: blk[4 + i // 2] |= code << 4
            else: blk[4 + i // 2] = code
        out += blk
    return bytes(out)


def read_wav(path):
    d = open(path, 'rb').read()
    pos, fmt = 12, None
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack_from('<I', d, pos + 4)[0]
        body = d[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', body)
        elif cid == b'data':
            tag, ch, rate, bits = fmt[0], fmt[1], fmt[2], fmt[5]
            bps = bits // 8
            n = len(body) // (bps * ch)
            x = []
            for i in range(n):
                s = 0.0
                for c in range(ch):
                    o = (i * ch + c) * bps
                    if tag == 3 and bits == 32: s += struct.unpack_from('<f', body, o)[0]
                    elif bits == 16: s += struct.unpack_from('<h', body, o)[0] / 32768
                    elif bits == 24: s += int.from_bytes(body[o:o + 3], 'little', signed=True) / 8388608
                    elif bits == 8: s += (body[o] - 128) / 128
                x.append(s / ch)
            return x, rate
        pos += 8 + size + (size & 1)
    raise ValueError(path + ': no audio')


def resample(x, src, dst):
    if src == dst: return x
    n = int(len(x) * dst / src)
    out = []
    for i in range(n):
        p = i * src / dst
        k = int(p); f = p - k
        a = x[k]; b = x[k + 1] if k + 1 < len(x) else 0.0
        out.append(a + (b - a) * f)
    return out


# ---- generated demo sounds (deterministic) ----
def env(t, a, d):
    return min(1.0, t / a) if t < a else math.exp(-(t - a) / d)


def demo_break(bpm=120):
    rnd = random.Random(7); spb = 60 / bpm; n = int(8 * spb * RATE); x = [0.0] * n
    def hit(at, fn, length):
        s = int(at * RATE)
        for i in range(int(length * RATE)):
            if s + i < n: x[s + i] += fn(i / RATE)
    kick = lambda t: math.sin(2 * math.pi * (50 * t + 110 * (1 - math.exp(-t * 30)) / 30)) * math.exp(-t * 9) * 0.9
    snare = lambda t: (rnd.uniform(-1, 1) * 0.6 + math.sin(2 * math.pi * 190 * t) * 0.4) * math.exp(-t * 16) * 0.7
    hat = lambda t: rnd.uniform(-1, 1) * math.exp(-t * 60) * 0.25
    pattern = [(0, 'k'), (0.75, 'k'), (1, 's'), (1.75, 'k'), (2.5, 'k'), (3, 's'), (3.25, 's')]
    for bar in range(2):
        for at, w in pattern:
            hit((bar * 4 + at) * spb, kick if w == 'k' else snare, 0.4)
        for h in range(8):
            hit((bar * 4 + h * 0.5) * spb, hat, 0.08)
    prev = 0.0
    for i in range(n):          # a touch of high-pass on the hats' noise
        prev = 0.97 * prev + 0.03 * x[i]
    return x, bpm


def saw(ph): return 2 * (ph - math.floor(ph + 0.5))


def demo_pad():
    n = int(2.5 * RATE); x = []; notes = [57, 60, 64, 67]; lp = 0.0
    for i in range(n):
        t = i / RATE; s = 0.0
        for k, m in enumerate(notes):
            f = 440 * 2 ** ((m - 69) / 12)
            s += saw(f * t * 1.003) + saw(f * t * 0.997 + k * 0.1)
        lp += (s / 8 - lp) * 0.08
        x.append(lp * min(1, t / 0.5) * min(1, (2.5 - t) / 0.4) * 0.8)
    return x, 0


def demo_bass(bpm=120):
    spb = 60 / bpm; n = int(4 * spb * RATE); x = [0.0] * n
    seq = [33, 33, 45, 33, 36, 33, 40, 38]
    for k, m in enumerate(seq):
        s = int(k * spb / 2 * RATE); f = 440 * 2 ** ((m - 69) / 12); lp = 0.0
        for i in range(int(spb / 2 * RATE)):
            t = i / RATE
            lp += (saw(f * t) - lp) * (0.02 + 0.3 * math.exp(-t * 18))
            if s + i < n: x[s + i] = lp * math.exp(-t * 4) * 0.8
    return x, bpm


def demo_bell():
    n = 2 * RATE; x = []
    for i in range(n):
        t = i / RATE
        m = math.sin(2 * math.pi * 440 * 3.5 * t) * 3 * math.exp(-t * 2)
        x.append(math.sin(2 * math.pi * 440 * t + m) * math.exp(-t * 1.3) * 0.6)
    return x, 0


def demo_vox():
    n = 2 * RATE; x = []; st = [0.0, 0.0, 0.0, 0.0]
    forms = [(800, 0.03), (1150, 0.04), (2900, 0.06)]
    for i in range(n):
        t = i / RATE
        src = 1.0 if (t * 140 * (1 + 0.01 * math.sin(t * 30))) % 1 < 0.08 else 0.0
        y = 0.0
        for k, (f, bw) in enumerate(forms):
            r = math.exp(-math.pi * bw * f / RATE * 20)
            c = 2 * r * math.cos(2 * math.pi * f / RATE)
            v = src + c * st[k] - r * r * (x[-2] if False else 0)
            st[k] = v * 0.5 + st[k] * 0.5
            y += math.sin(2 * math.pi * f * t) * src
        x.append(0.0)
    # simpler: pulse train through three resonators (two-pole)
    x = []; mem = [[0.0, 0.0] for _ in forms]
    for i in range(n):
        t = i / RATE
        src = 1.0 if (t * 140 * (1 + 0.015 * math.sin(t * 31))) % 1 < 0.05 else 0.0
        y = 0.0
        for k, (f, bw) in enumerate(forms):
            r = 1 - math.pi * f * bw / RATE * 4
            c = 2 * r * math.cos(2 * math.pi * f / RATE)
            v = src * (1 - r) * 4 + c * mem[k][0] - r * r * mem[k][1]
            mem[k][1] = mem[k][0]; mem[k][0] = v
            y += v / (k + 1)
        x.append(y * 0.3 * min(1, t / 0.15) * min(1, (2 - t) / 0.3))
    peak = max(abs(v) for v in x) or 1
    return [v / peak * 0.7 for v in x], 0


def demo_texture():
    rnd = random.Random(3); n = 2 * RATE; x = []; a = b = 0.0
    for i in range(n):
        t = i / RATE
        w = rnd.uniform(-1, 1)
        cut = 0.02 + 0.15 * (0.5 + 0.5 * math.sin(t * 2.1))
        a += (w - a) * cut; b += (a - b) * cut
        x.append(b * 2.5 * (0.4 + 0.6 * abs(math.sin(t * 1.3))))
    return x, 0


def demo_arp(bpm=120):
    spb = 60 / bpm; n = int(4 * spb * RATE); x = [0.0] * n
    notes = [69, 72, 76, 79, 81, 79, 76, 72]
    for k in range(16):
        f = 440 * 2 ** ((notes[k % 8] - 69) / 12); s = int(k * spb / 4 * RATE)
        for i in range(int(spb / 4 * RATE)):
            t = i / RATE
            if s + i < n: x[s + i] = (1 if (f * t) % 1 < 0.5 else -1) * math.exp(-t * 14) * 0.35
    return x, bpm


def demo_clap():
    rnd = random.Random(11); n = int(0.6 * RATE); x = []
    for i in range(n):
        t = i / RATE
        e = sum(math.exp(-(t - d) * 120) for d in (0, 0.011, 0.022) if t >= d) + 0.6 * math.exp(-t * 14)
        x.append(rnd.uniform(-1, 1) * e * 0.35)
    return x, 0


DEMOS = [('BREAK', demo_break), ('PAD', demo_pad), ('BASS', demo_bass), ('BELL', demo_bell),
         ('VOX', demo_vox), ('TEXTURE', demo_texture), ('ARP', demo_arp), ('CLAP', demo_clap)]


def main():
    budget = 300 * 1024
    if '--budget-kb' in sys.argv:
        budget = int(sys.argv[sys.argv.index('--budget-kb') + 1]) * 1024
    folder = os.path.join(ROOT, 'samples')
    files = sorted(f for f in os.listdir(folder) if f.lower().endswith('.wav'))[:8] if os.path.isdir(folder) else []
    slots = []
    for k in range(8):
        if k < len(files):
            x, rate = read_wav(os.path.join(folder, files[k]))
            x = resample(x, rate, RATE)
            m = re.search(r'(\d{2,3})\s*bpm', files[k], re.I)
            name = re.sub(r'[^A-Z0-9 ]', '', os.path.splitext(files[k])[0].upper())[:12].strip() or 'SAMPLE'
            slots.append((name, x, float(m.group(1)) if m else 0.0))
        else:
            name, fn = DEMOS[k]
            x, bpm = fn()
            slots.append((name, x, float(bpm)))
    # fit the budget: shorten the longest slots first
    per_sec = RATE // BLOCK * (4 + BLOCK // 2)
    while sum(len(x) for _, x, _ in slots) / RATE * per_sec > budget:
        i = max(range(8), key=lambda j: len(slots[j][1]))
        name, x, bpm = slots[i]
        keep = int(len(x) * 0.9)
        if bpm:                                   # loops keep whole beats
            beat = int(RATE * 60 / bpm)
            keep = max(beat, keep // beat * beat)
        slots[i] = (name, x[:keep], bpm)
    out = ['/* Generated by tools/make_bank.py: Chisel\'s built-in samples. Do not edit. */',
           '#pragma once', '#include "chisel.h"', '']
    total = 0
    for k, (name, x, bpm) in enumerate(slots):
        peak = max((abs(v) for v in x), default=1) or 1
        g = 0.89 / peak if peak > 0.89 else 1.0
        pcm = [max(-32768, min(32767, int(round(v * g * 32767)))) for v in x]
        data = adpcm(pcm)
        total += len(data)
        out.append('static const uint8_t bank%d[%d] __attribute__((aligned(4))) = {' % (k, len(data)))
        for i in range(0, len(data), 24):
            out.append('    ' + ','.join(str(b) for b in data[i:i + 24]) + ',')
        out.append('};')
        print('slot %d %-12s %6.2f s  %3.0f bpm  %6d bytes' % (k + 1, name, len(x) / RATE, bpm, len(data)))
    out.append('static const char *const bank_name[8] = {%s};' % ', '.join('"%s"' % s[0] for s in slots))
    out.append('static const ch_sample bank[8] = {')
    for k, (name, x, bpm) in enumerate(slots):
        out.append('    {bank%d, %d, %d, %.1ff},' % (k, len(x), RATE, bpm))
    out.append('};')
    open(os.path.join(ROOT, 'app', 'bank.h'), 'w').write('\n'.join(out) + '\n')
    print('bank: %d KB of %d KB' % (total // 1024, budget // 1024))


main()
