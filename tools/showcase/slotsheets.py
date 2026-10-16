import cv2, numpy as np, glob, os, re, sys, collections
capdir, log, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
frames = sorted((int(os.path.splitext(os.path.basename(f))[0]), f) for f in glob.glob(os.path.join(capdir, '*.jpg')))
steps = []; verdicts = {}
for line in open(log, encoding='utf-8', errors='replace'):
    m = re.search(r'Showcase :: wall=(\d+) master=(\d+) slot=(\w+) skill=(\d+) caster=\S+ range=(\d+) dist=(\d+) test=(\S+)', line)
    if m: steps.append(m.groups()); continue
    v = re.search(r'Showcase :: result hit=(\d) expect=(-?\d) (\w+) damage=(-?\d+)(?: hits=(\d+))?', line)
    if v and steps: verdicts[len(steps) - 1] = v.groups()
groups = collections.OrderedDict()
for i, s in enumerate(steps):
    end = int(steps[i + 1][0]) if i + 1 < len(steps) else int(s[0]) + 3500
    groups.setdefault((int(s[1]), s[2], s[3]), []).append((i, s, end))
COLS = 6
for (master, slot, skill), lst in groups.items():
    if slot == 'TAG' or (slot != 'WEAPON' and skill == '0'): continue
    rows = []
    for i, (wall, _, _, _, rng, dist, test), end in lst:
        sel = [f for t, f in frames if int(wall) - 100 <= t <= end]
        if not sel: continue
        idx = np.linspace(0, len(sel) - 1, min(COLS, len(sel))).astype(int)
        cells = [cv2.resize(cv2.imread(sel[k]), (480, 270)) for k in idx]
        while len(cells) < COLS: cells.append(np.zeros((270, 480, 3), np.uint8))
        row = np.hstack(cells)
        v = verdicts.get(i, ('', '', '', '', ''))
        label = np.zeros((30, row.shape[1], 3), np.uint8)
        col = (0, 0, 255) if v[2] == 'MISMATCH' else (0, 255, 255)
        cv2.putText(label, f'{slot} {skill} {test} range={rng} dist={dist} hit={v[0]} expect={v[1]} {v[2]} dmg={v[3]} hits={v[4]}', (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.7, col, 2)
        rows.append(np.vstack([label, row]))
    if rows:
        cv2.imwrite(os.path.join(out, f'm{master:02d}_{slot}.jpg'), np.vstack(rows), [cv2.IMWRITE_JPEG_QUALITY, 70])
        open(os.path.join(out, f'm{master:02d}_{slot}.jpg.skill'), 'w').write(skill)
print('done')
