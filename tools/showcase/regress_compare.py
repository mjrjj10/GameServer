import collections
import re
import sys

DAMAGE = re.compile(r'Damage :: (.+?) -> (.+?) skill=(-?\d+) damage=(\d+)(?: CRIT)? hp=\S+ in=(\d+) remote=(\d+)')
STATUS = re.compile(r'Status :: (.+?) -> (.+?) status=(\d+) type=')
CAST = re.compile(r'Skill :: (.+?) skill=(\d+) ')
DEATH = re.compile(r'Death :: ')
MIN_HITS = 3
HIT_RATIO = 0.35
DAMAGE_RATIO = 0.25


def scan(path):
    casts, hits, dmg, raw, statuses, alerts = (collections.Counter() for _ in range(6))
    for line in open(path, encoding='utf-8', errors='replace'):
        m = DAMAGE.search(line)
        if m:
            key = (int(m.group(3)), int(m.group(6)))
            hits[key] += 1
            dmg[key] += int(m.group(4))
            raw[key] += int(m.group(5))
            continue
        m = STATUS.search(line)
        if m:
            statuses[int(m.group(3))] += 1
            continue
        m = CAST.search(line)
        if m:
            casts[int(m.group(2))] += 1
            continue
        if re.search(r'ASSERT|assert|crash|Exception', line):
            alerts[line.strip()[:160]] += 1
    return casts, hits, dmg, raw, statuses, alerts


def differs(a, b, ratio):
    if max(a, b) < MIN_HITS:
        return False
    return abs(a - b) > ratio * max(a, b)


def main():
    base, new, out = sys.argv[1], sys.argv[2], sys.argv[3]
    bc, bh, bd, br, bs, ba = scan(base)
    nc, nh, nd, nr, ns, na = scan(new)
    lines = ['# Regression %s vs %s' % (base, new), '']
    lines.append('casts base=%d new=%d, damage lines base=%d new=%d, statuses base=%d new=%d' % (sum(bc.values()), sum(nc.values()), sum(bh.values()), sum(nh.values()), sum(bs.values()), sum(ns.values())))
    lines.append('')
    lines.append('## Skills cast in one run only')
    for k in sorted(set(bc) ^ set(nc)):
        if (bc[k] if k in bc else nc[k]) >= 2:
            lines.append('- skill %d base=%d new=%d' % (k, bc[k], nc[k]))
    lines.append('')
    lines.append('## Hits / damage per cast (skill, remote)')
    for k in sorted(set(bh) | set(nh)):
        s = k[0]
        bpc = bh[k] / max(bc[s], 1) if s > 0 else bh[k]
        npc = nh[k] / max(nc[s], 1) if s > 0 else nh[k]
        bdp = bd[k] / max(bh[k], 1)
        ndp = nd[k] / max(nh[k], 1)
        flag = []
        if differs(bpc * 100, npc * 100, HIT_RATIO) and max(bh[k], nh[k]) >= MIN_HITS:
            flag.append('hits/cast %.2f -> %.2f' % (bpc, npc))
        if bh[k] >= MIN_HITS and nh[k] >= MIN_HITS and abs(bdp - ndp) > DAMAGE_RATIO * max(bdp, ndp):
            flag.append('dmg/hit %.0f -> %.0f' % (bdp, ndp))
        if (bh[k] == 0) != (nh[k] == 0) and max(bh[k], nh[k]) >= MIN_HITS:
            flag.append('only in %s (%d hits)' % ('base' if bh[k] else 'new', max(bh[k], nh[k])))
        if flag:
            lines.append('- skill %d remote %d: %s' % (k[0], k[1], ', '.join(flag)))
    lines.append('')
    lines.append('## Statuses applied in one run only (>= %d)' % MIN_HITS)
    for k in sorted(set(bs) | set(ns)):
        if (bs[k] == 0) != (ns[k] == 0) and max(bs[k], ns[k]) >= MIN_HITS:
            lines.append('- status %d base=%d new=%d' % (k, bs[k], ns[k]))
    lines.append('')
    lines.append('## Alerts')
    for k, v in na.most_common(20):
        lines.append('- new x%d %s' % (v, k))
    for k, v in ba.most_common(10):
        lines.append('- base x%d %s' % (v, k))
    open(out, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[:60]))


if __name__ == '__main__':
    main()
