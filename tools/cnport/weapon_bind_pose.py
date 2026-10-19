import os
import struct
import sys

MOD = r'D:\MxM\ClientMod\Data'
CN = r'D:\MxM\ClientCN\cn\Data'


def header(d):
    i = d.index(b'\n') + 1
    i += 4 + 1 + 4
    nb, = struct.unpack_from('<I', d, i)
    i += 4
    nt, = struct.unpack_from('<H', d, i)
    i += 2
    types = []
    for _ in range(nt):
        l, = struct.unpack_from('<I', d, i)
        i += 4
        types.append(d[i:i + l].decode())
        i += l
    idx = struct.unpack_from('<%dH' % nb, d, i)
    i += 2 * nb
    sizes = struct.unpack_from('<%dI' % nb, d, i)
    i += 4 * nb
    ns, = struct.unpack_from('<I', d, i)
    i += 8
    strs = []
    for _ in range(ns):
        l, = struct.unpack_from('<I', d, i)
        i += 4
        strs.append(d[i:i + l].decode('latin1'))
        i += l
    ng, = struct.unpack_from('<I', d, i)
    i += 4 + 4 * ng
    offs = []
    for s in sizes:
        offs.append(i)
        i += s
    return [types[x & 0x7fff] for x in idx], sizes, offs, strs


def name_of(d, offs, strs, b):
    k = struct.unpack_from('<i', d, offs[b])[0]
    return strs[k] if 0 <= k < len(strs) else ''


def kf_pose(path, node):
    d = open(path, 'rb').read()
    bt, sizes, offs, strs = header(d)
    data = [b for b, t in enumerate(bt) if t == 'NiBSplineData'][0]
    o = offs[data]
    nf, = struct.unpack_from('<I', d, o)
    o += 4 + 4 * nf
    nc, = struct.unpack_from('<I', d, o)
    sh = struct.unpack_from('<%dh' % nc, d, o + 4)
    for b, t in enumerate(bt):
        if t != 'NiBSplineCompTransformEvaluator' or name_of(d, offs, strs, b) != node:
            continue
        o = offs[b]
        tr = struct.unpack_from('<3f', d, o + 40)
        rot = struct.unpack_from('<4f', d, o + 52)
        th, rh = struct.unpack_from('<2I', d, o + 72)
        to, thr, ro, rhr = struct.unpack_from('<4f', d, o + 84)
        if th != 0xffffffff:
            tr = tuple(to + thr * sh[th + i] / 32767.0 for i in range(3))
        if rh != 0xffffffff:
            rot = tuple(ro + rhr * sh[rh + i] / 32767.0 for i in range(4))
        n = sum(x * x for x in rot) ** 0.5
        return tr, tuple(x / n for x in rot)
    return None


def matrix(w, x, y, z):
    return (1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
            2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
            2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y))


def main():
    nif_rel, kf_rel = sys.argv[1], sys.argv[2]
    nodes = sys.argv[3:]
    src = os.path.join(CN, nif_rel)
    dst = os.path.join(MOD, nif_rel)
    d = bytearray(open(src, 'rb').read())
    bt, sizes, offs, strs = header(bytes(d))
    for node in nodes:
        p = kf_pose(os.path.join(CN, kf_rel), node)
        for b, t in enumerate(bt):
            if t != 'NiNode' or name_of(d, offs, strs, b) != node:
                continue
            o = offs[b]
            ne, = struct.unpack_from('<I', d, o + 4)
            o += 8 + 4 * ne + 4 + 2
            old = struct.unpack_from('<3f', d, o)
            struct.pack_into('<3f', d, o, *p[0])
            struct.pack_into('<9f', d, o + 12, *matrix(*p[1]))
            print('%s %s (%.0f, %.0f, %.0f) -> (%.0f, %.0f, %.0f)' % (nif_rel, node, *old, *p[0]))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, 'wb').write(bytes(d))


if __name__ == '__main__':
    main()
