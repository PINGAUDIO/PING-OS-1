"""Look at a bench render without listening: level over time, click count,
and a log-frequency spectrogram PNG. Standard library only.

    python -I dev/analyze.py dev/out/ring.wav [more.wav ...]
"""
import cmath, math, struct, sys, zlib


def read_wav(path):
    d = open(path, 'rb').read()
    pos, fmt = 12, None
    while pos < len(d):
        cid, size = d[pos:pos + 4], struct.unpack_from('<I', d, pos + 4)[0]
        body = d[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', body)
        elif cid == b'data':
            ch, rate, bits = fmt[1], fmt[2], fmt[5]
            bps = bits // 8
            n = len(body) // (bps * ch)
            left = []
            for i in range(n):
                o = i * bps * ch
                if bits == 24:
                    v = int.from_bytes(body[o:o + 3], 'little', signed=True) / 8388608
                else:
                    v = struct.unpack_from('<h', body, o)[0] / 32768
                left.append(v)
            return left, rate
        pos += 8 + size + (size & 1)
    raise ValueError('no data')


def fft(x):
    n = len(x)
    if n == 1:
        return x
    even, odd = fft(x[0::2]), fft(x[1::2])
    t = [cmath.exp(-2j * math.pi * k / n) * odd[k] for k in range(n // 2)]
    return [even[k] + t[k] for k in range(n // 2)] + [even[k] - t[k] for k in range(n // 2)]


def png(path, w, h, rows):
    raw = b''.join(b'\x00' + bytes(r) for r in rows)
    def chunk(t, data):
        c = struct.pack('>I', len(data)) + t + data
        return c + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
    out = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
    open(path, 'wb').write(out)


def colour(v):
    v = max(0.0, min(1.0, v))
    return (int(255 * min(1, v * 1.6)), int(255 * max(0, min(1, v * 1.6 - 0.45))), int(255 * max(0, v * 2 - 1.2) + 40 * (1 - v)))


def analyse(path):
    x, rate = read_wav(path)
    n = len(x)
    # level timeline, 0.25 s windows
    win = rate // 4
    levels = []
    for i in range(0, n - win + 1, win):
        s = x[i:i + win]
        r = math.sqrt(sum(v * v for v in s) / win)
        levels.append(-120 if r <= 0 else 20 * math.log10(r))
    # clicks: second differences far above the local level
    clicks, last, where = 0, -rate, []
    blk = 512
    for b in range(0, n - blk, blk):
        s = x[b:b + blk]
        rms = math.sqrt(sum(v * v for v in s) / blk) + 1e-4
        for i in range(b + 2, b + blk):
            d2 = abs(x[i] - 2 * x[i - 1] + x[i - 2])
            if d2 > 0.25 and d2 > 6 * rms and i - last > rate // 50:
                clicks += 1; last = i; where.append(round(i / rate, 3))
    # spectrogram: 1024-point frames, log frequency 30 Hz .. 20 kHz
    N, hop, H = 1024, max(1, n // 600), 160
    cols = []
    for start in range(0, n - N, hop):
        frame = [x[start + k] * (0.5 - 0.5 * math.cos(2 * math.pi * k / N)) for k in range(N)]
        spec = fft(frame)
        mag = [abs(spec[k]) for k in range(N // 2)]
        col = []
        for yy in range(H):
            f = 30 * (20000 / 30) ** (yy / (H - 1))
            k = min(N // 2 - 1, int(f * N / rate))
            db = 20 * math.log10(mag[k] + 1e-9)
            col.append((db + 80) / 80)
        cols.append(col)
    w = len(cols)
    rows = []
    for yy in range(H - 1, -1, -1):
        row = []
        for c in cols:
            row.extend(colour(c[yy]))
        rows.append(row)
    out = path[:-4] + '.png'
    png(out, w, H, rows)
    lv = ' '.join('%d' % round(v) for v in levels)
    print('%s  %.1fs  peak %.2f  clicks %d %s\n  dB/0.25s: %s\n  -> %s' % (path, n / rate, max(abs(v) for v in x), clicks, where[:8], lv, out))


for p in sys.argv[1:]:
    analyse(p)
