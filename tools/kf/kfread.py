import struct
import sys


class Reader:
    def __init__(self, b, o=0):
        self.b = b
        self.o = o

    def u8(self):
        v = self.b[self.o]
        self.o += 1
        return v

    def u16(self):
        v = struct.unpack_from('<H', self.b, self.o)[0]
        self.o += 2
        return v

    def u32(self):
        v = struct.unpack_from('<I', self.b, self.o)[0]
        self.o += 4
        return v

    def i32(self):
        v = struct.unpack_from('<i', self.b, self.o)[0]
        self.o += 4
        return v

    def f32(self):
        v = struct.unpack_from('<f', self.b, self.o)[0]
        self.o += 4
        return v

    def raw(self, n):
        v = self.b[self.o:self.o + n]
        self.o += n
        return v


def parse(path):
    b = open(path, 'rb').read()
    nl = b.index(b'\n')
    r = Reader(b, nl + 1)
    version = r.u32()
    endian = r.u8()
    user = r.u32()
    nblocks = r.u32()
    ntypes = r.u16()
    types = [r.raw(r.u32()).decode() for _ in range(ntypes)]
    tindex = [r.u16() for _ in range(nblocks)]
    sizes = [r.u32() for _ in range(nblocks)]
    nstrings = r.u32()
    r.u32()
    strings = [r.raw(r.u32()).decode('latin1') for _ in range(nstrings)]
    ngroups = r.u32()
    r.o += 4 * ngroups
    blocks = []
    for i in range(nblocks):
        blocks.append((types[tindex[i] & 0x7fff], r.o, sizes[i]))
        r.o += sizes[i]
    return b, hex(version), strings, blocks


if __name__ == '__main__':
    b, v, strings, blocks = parse(sys.argv[1])
    print(v, len(strings), strings[:40])
    for i, (t, o, s) in enumerate(blocks):
        print(i, t, o, s)


def evaluator_name(b, strings, off):
    r = Reader(b, off)
    idx = r.i32()
    return strings[idx] if 0 <= idx < len(strings) else None


def bspline_comp_evaluator(b, off):
    r = Reader(b, off)
    r.o += 5 * 4
    r.o += 4
    start = r.f32()
    end = r.f32()
    data = r.i32()
    basis = r.i32()
    t = [r.f32() for _ in range(3)]
    q = [r.f32() for _ in range(4)]
    s = r.f32()
    th = r.u32()
    rh = r.u32()
    sh = r.u32()
    toff = r.f32()
    thalf = r.f32()
    return dict(start=start, end=end, data=data, basis=basis, t=t, th=th, rh=rh, sh=sh, toff=toff, thalf=thalf)


def bspline_data(b, off):
    r = Reader(b, off)
    nf = r.u32()
    floats = [r.f32() for _ in range(nf)]
    ns = r.u32()
    shorts = [struct.unpack_from('<h', b, r.o + 2 * i)[0] for i in range(ns)]
    return floats, shorts


def basis_count(b, off):
    return Reader(b, off).u32()


def basis_weights(n, u):
    degree = 3
    knots = [0.0] * (degree + 1) + [float(i) for i in range(1, n - degree)] + [float(n - degree)] * (degree + 1)
    x = u * (n - degree)
    if x >= n - degree:
        x = n - degree - 1e-6
    w = [0.0] * n
    span = degree
    while span < n - 1 and knots[span + 1] <= x:
        span += 1
    N = [0.0] * (degree + 1)
    N[0] = 1.0
    left = [0.0] * (degree + 1)
    right = [0.0] * (degree + 1)
    for j in range(1, degree + 1):
        left[j] = x - knots[span + 1 - j]
        right[j] = knots[span + j] - x
        saved = 0.0
        for k in range(j):
            denom = right[k + 1] + left[j - k]
            tmp = N[k] / denom if denom != 0 else 0.0
            N[k] = saved + right[k + 1] * tmp
            saved = left[j - k] * tmp
        N[j] = saved
    for k in range(degree + 1):
        w[span - degree + k] = N[k]
    return w


def root_translation(path, node='Bip01', samples=31):
    b, v, strings, blocks = parse(path)
    for i, (t, o, s) in enumerate(blocks):
        if t != 'NiBSplineCompTransformEvaluator' or evaluator_name(b, strings, o) != node:
            continue
        ev = bspline_comp_evaluator(b, o)
        if ev['th'] == 0xffffffff:
            return ev, [(k / (samples - 1), ev['t']) for k in range(samples)]
        floats, shorts = bspline_data(b, blocks[ev['data']][1])
        n = basis_count(b, blocks[ev['basis']][1])
        out = []
        for k in range(samples):
            u = k / (samples - 1)
            w = basis_weights(n, u)
            p = [0.0, 0.0, 0.0]
            for c in range(n):
                if w[c] == 0:
                    continue
                for a in range(3):
                    p[a] += w[c] * (ev['toff'] + shorts[ev['th'] + 3 * c + a] / 32767.0 * ev['thalf'])
            out.append((ev['start'] + u * (ev['end'] - ev['start']), p))
        return ev, out
    return None, []


def transform_evaluator(b, blocks, off):
    r = Reader(b, off)
    r.o += 24
    t = [r.f32() for _ in range(3)]
    r.o += 20
    data = r.i32()
    keys = []
    if data >= 0:
        r = Reader(b, blocks[data][1])
        nrot = r.u32()
        if nrot:
            rtype = r.u32()
            if rtype == 4:
                raise ValueError('xyz rotation keys')
            per = {1: 20, 2: 20, 3: 32}[rtype]
            r.o += nrot * per
        ntr = r.u32()
        if ntr:
            ttype = r.u32()
            for _ in range(ntr):
                tm = r.f32()
                v = [r.f32() for _ in range(3)]
                if ttype == 2:
                    r.o += 24
                elif ttype == 3:
                    r.o += 12
                keys.append((tm, v))
    return t, keys


def node_translation_keys(path, node):
    b, v, strings, blocks = parse(path)
    for i, (t, o, s) in enumerate(blocks):
        if t == 'NiTransformEvaluator' and evaluator_name(b, strings, o) == node:
            return transform_evaluator(b, blocks, o)
    return None, []
