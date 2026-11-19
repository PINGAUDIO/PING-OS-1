"""A PING FM1 sample bank from WAV files, as app/web/loader.html makes one in the browser (app/samples.h has the
format): the payload the FM-1 holds from 0xCA000, with its commit record for the simulator's flash file.
    python tools/make_samplebank.py OUT.bin [--rate 32000] NAME=file.wav ...     (up to 8; a name of 13 characters)
    python tools/make_samplebank.py OUT.bin --flash FLASH.bin ...                 (the record and payload into a sim flash image)
Each file is mixed to mono, resampled to the rate (linear), encoded (the engine's greedy IMA ADPCM, 64-frame
blocks). The payload is at most 73,728 bytes: about 4.6 s at 32 kHz, 9.2 s at 16 kHz, in all."""
import struct, sys, wave, zlib
from pathlib import Path

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
        118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
        1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
        6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]
BLOCK = 64


def step_decode(code, p, ix):
    st = STEP[ix]; d = st >> 3
    if code & 4: d += st
    if code & 2: d += st >> 1
    if code & 1: d += st >> 2
    p = p - d if code & 8 else p + d
    p = max(-32768, min(32767, p))
    ix = max(0, min(88, ix + INDEX[code]))
    return p, ix


def encode(pcm):
    out = bytearray(); ix = 0
    for start in range(0, len(pcm), BLOCK):
        p = pcm[start]
        b = bytearray(4 + BLOCK // 2); b[0] = p & 255; b[1] = (p >> 8) & 255; b[2] = ix; b[3] = 0
        for i in range(BLOCK):
            target = pcm[start + i] if start + i < len(pcm) else 0
            best, be, bp, bi = 0, 1 << 30, 0, 0
            for code in range(16):
                q, j = step_decode(code, p, ix)
                e = abs(q - target)
                if e < be: be, best, bp, bi = e, code, q, j
            p, ix = bp, bi
            if i & 1: b[4 + i // 2] |= best << 4
            else: b[4 + i // 2] = best
        out += b
    return bytes(out)


def read_wav(path, rate):
    w = wave.open(str(path)); ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n); w.close()
    frames = []
    for i in range(n):
        acc = 0
        for c in range(ch):
            o = (i * ch + c) * sw
            v = int.from_bytes(raw[o:o + sw], 'little', signed=True) if sw > 1 else raw[o] - 128 << 8
            acc += v >> (8 * (sw - 2)) if sw > 2 else v
        frames.append(acc // ch)
    if sr == rate: return frames
    out = []; r = sr / rate; k = int(len(frames) / r)
    for i in range(k):
        x = i * r; j = int(x); f = x - j
        a = frames[j]; b = frames[j + 1] if j + 1 < len(frames) else a
        out.append(int(a + (b - a) * f))
    return out


def main():
    args = sys.argv[1:]
    if not args: sys.exit(__doc__)
    out = Path(args.pop(0)); rate = 32000; flash = None
    items = []
    while args:
        a = args.pop(0)
        if a == '--rate': rate = int(args.pop(0))
        elif a == '--flash': flash = Path(args.pop(0))
        else:
            name, _, f = a.partition('=')
            items.append((name[:13], Path(f), rate))
    if len(items) > 8: sys.exit('at most 8 samples')
    table = bytearray(struct.pack('<II', 0x42504D53, len(items))); data = bytearray(); entries = []
    off = 8 + 8 * 32
    for name, f, r in items:
        pcm = read_wav(f, r); enc = encode(pcm)
        entries.append(struct.pack('<IIIIH14s', off + len(data), len(enc), len(pcm), r, 0, name.encode('ascii', 'replace')[:13]))
        data += enc
        print(f'{name:14s} {f.name}: {len(pcm)} frames at {r} Hz, {len(enc)} B')
    while len(entries) < 8: entries.append(bytes(32))
    payload = bytes(table) + b''.join(entries) + bytes(data)
    if len(payload) > 73728: sys.exit(f'the bank is {len(payload)} bytes: over 73,728. Fewer samples, a lower rate, or shorter')
    crc = zlib.crc32(payload)
    out.write_bytes(payload)
    print(f'{out}: {len(payload)} B of 73,728 ({len(payload) * 100 // 73728}%), crc {crc:08x}')
    if flash:
        img = bytearray(flash.read_bytes()) if flash.exists() else bytearray(b'\xff' * 0x100000)
        rec = struct.pack('<III', 0x31504D53, len(payload), crc); rec += struct.pack('<I', zlib.crc32(rec))
        img[0xC9000:0xC9000 + 0x1000] = b'\xff' * 0x1000; img[0xC9000:0xC9000 + 16] = rec
        img[0xCA000:0xCA000 + 73728] = b'\xff' * 73728; img[0xCA000:0xCA000 + len(payload)] = payload
        flash.write_bytes(img); print(f'{flash}: the record and payload written')


if __name__ == '__main__':
    main()
