import cv2, glob, os, re, sys, collections
import numpy as np

runs, tag, previews, out = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
os.makedirs(out, exist_ok=True)
W, H, COLS = 320, 180, 6
SLOTS = ['WEAPON', 'SKILL_1', 'SKILL_2', 'SKILL_3', 'SKILL_4', 'ULTIMATE', 'SHIRK']


def preview_cells(skill):
    p = os.path.join(previews, f'{skill}.mp4')
    cap = cv2.VideoCapture(p)
    n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT)) if cap.isOpened() else 0
    if n <= 0:
        return None
    cells = []
    for k in np.linspace(n * 0.1, n * 0.9, COLS).astype(int):
        cap.set(cv2.CAP_PROP_POS_FRAMES, int(k))
        ok, f = cap.read()
        cells.append(cv2.resize(f, (W, H)) if ok else np.zeros((H, W, 3), np.uint8))
    return cells


def action_cells(sel):
    if not sel:
        return []
    imgs = [cv2.imread(f) for f in sel]
    small = [cv2.cvtColor(cv2.resize(im, (480, 270)), cv2.COLOR_BGR2GRAY).astype(np.int16) for im in imgs]
    acc = np.zeros((270, 480), np.float32)
    act = [0.0]
    for k in range(1, len(small)):
        d = np.abs(small[k] - small[k - 1]).astype(np.float32)
        d[:30, :] = 0
        d[int(270 * 0.83):, :] = 0
        d[:, :45] = 0
        acc += d
        act.append(float(d.mean()))
    ys, xs = np.where(acc > max(acc.max() * 0.25, 20))
    if len(xs) > 20:
        cx, cy = int(np.median(xs)) * 4, int(np.median(ys)) * 4
    else:
        cx, cy = 960, 540
    cw, ch = 800, 450
    x0 = min(max(cx - cw // 2, 0), 1920 - cw)
    y0 = min(max(cy - ch // 2, 0), 1080 - ch)
    order = sorted(range(len(imgs)), key=lambda k: -act[k])[:COLS]
    order.sort()
    return [cv2.resize(imgs[k][y0:y0 + ch, x0:x0 + cw], (W, H)) for k in order]


def label(text, width, color=(0, 255, 255)):
    img = np.zeros((26, width, 3), np.uint8)
    cv2.putText(img, text, (6, 19), cv2.FONT_HERSHEY_SIMPLEX, 0.55, color, 1)
    return img


for g in range(4):
    log = os.path.join(runs, f'{tag}_g{g}.log')
    capdir = os.path.join(runs, f'cap_{tag}_g{g}')
    if not os.path.exists(log):
        continue
    frames = sorted((int(os.path.splitext(os.path.basename(f))[0]), f) for f in glob.glob(os.path.join(capdir, '*.jpg')))
    steps, verdict = [], {}
    for line in open(log, encoding='utf-8', errors='replace'):
        m = re.search(r'Showcase :: wall=(\d+) master=(\d+) slot=(\w+) skill=(\d+) caster=\S+ range=(\d+) dist=(\d+) test=(\S+)', line)
        if m:
            steps.append(m.groups())
            continue
        v = re.search(r'Showcase :: result hit=(\d) expect=(-?\d) (\w+) damage=(-?\d+)', line)
        if v and steps:
            verdict[len(steps) - 1] = v.groups()
    per = collections.OrderedDict()
    for i, s in enumerate(steps):
        end = int(steps[i + 1][0]) if i + 1 < len(steps) else int(s[0]) + 4000
        key = (int(s[1]), s[2])
        if key not in per or s[6] in ('near',) and per[key][1][6] != 'near':
            per[key] = (i, s, end)
    masters = collections.OrderedDict()
    for (m, slot), v in per.items():
        masters.setdefault(m, {})[slot] = v
    for m, slots in masters.items():
        rows = []
        for slot in SLOTS:
            if slot not in slots:
                continue
            i, s, end = slots[slot]
            wall, skill, test, dist = int(s[0]), s[3], s[6], s[5]
            sel = [f for t, f in frames if wall - 200 <= t <= end]
            ours = action_cells(sel)
            while len(ours) < COLS:
                ours.append(np.zeros((H, W, 3), np.uint8))
            pv = preview_cells(skill) if skill != '0' else None
            v = verdict.get(i, ('', '', '', ''))
            rows.append(label(f'{slot} {skill} test={test} dist={dist} hit={v[0]} expect={v[1]} {v[2]} dmg={v[3]}', W * COLS))
            if pv:
                rows.append(label('official preview', W * COLS, (255, 200, 0)))
                rows.append(np.hstack(pv))
            rows.append(label('server', W * COLS, (0, 255, 0)))
            rows.append(np.hstack(ours))
        if rows:
            cv2.imwrite(os.path.join(out, f'master_{m:02d}.jpg'), np.vstack(rows), [cv2.IMWRITE_JPEG_QUALITY, 72])
            print('master', m, 'slots', len(slots))
