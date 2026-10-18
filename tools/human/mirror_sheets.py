import cv2, glob, os, re, sys
import numpy as np

log, capdir, out = sys.argv[1], sys.argv[2], sys.argv[3]
step_ms = int(sys.argv[4]) if len(sys.argv) > 4 else 100
os.makedirs(out, exist_ok=True)
W, H, COLS = 400, 225, 6
CAMERA_CUT_MEAN = 12.0
WALL = re.compile(r'Showcase :: wall=(\d+) master=(\d+) slot=(\w+) skill=(\d+) caster=(\S+) range=\d+ dist=(\d+) test=(\S+)')
RES = re.compile(r'Showcase :: result hit=(\d) expect=(-?\d) (\w+) damage=(-?\d+)')
STATE = re.compile(r'Mirror :: human client state=(\S+) for ([0-9.]+)s casting=(-?\d+)( STUCK)?')

tests = []
for line in open(log, encoding='utf-8', errors='replace'):
    if m := WALL.search(line):
        tests.append(dict(wall=int(m.group(1)), master=int(m.group(2)), slot=m.group(3), skill=m.group(4), caster=m.group(5), dist=m.group(6), test=m.group(7), note=''))
    elif tests and (r := RES.search(line)):
        tests[-1]['note'] += f' hit={r.group(1)} {r.group(3)} dmg={r.group(4)}'
    elif tests and (s := STATE.search(line)):
        tests[-1]['note'] += f' state={s.group(1)} {s.group(2)}s' + (' STUCK' if s.group(4) else '')

frames = sorted((int(os.path.splitext(os.path.basename(f))[0]), f) for f in glob.glob(os.path.join(capdir, '*.jpg')))
stamps = np.array([t for t, _ in frames]) if frames else np.zeros(0)


def frame_at(t):
    i = int(np.searchsorted(stamps, t))
    i = min(max(i, 0), len(frames) - 1)
    if i > 0 and abs(stamps[i - 1] - t) < abs(stamps[i] - t):
        i -= 1
    return frames[i]


def label(text, width, color):
    img = np.zeros((24, width, 3), np.uint8)
    cv2.putText(img, text, (6, 17), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)
    return img


report = []
by_master = {}
allmotion = []
plans = []
for i, t in enumerate(tests):
    end = tests[i + 1]['wall'] if i + 1 < len(tests) else t['wall'] + 6000
    times = list(range(t['wall'], end + 1, step_ms))
    if not frames or len(times) < 2:
        continue
    imgs = [cv2.imread(frame_at(x)[1]) for x in times]
    fh, fw = imgs[0].shape[:2]
    small = [cv2.cvtColor(cv2.resize(im, (480, 270)), cv2.COLOR_BGR2GRAY).astype(np.int16) for im in imgs]
    acc = np.zeros((270, 480), np.float32)
    diffs = [None]
    cut = False
    for k in range(1, len(small)):
        d = np.abs(small[k] - small[k - 1]).astype(np.float32)
        if cut or float(d[30:int(270 * 0.83), 45:].mean()) > CAMERA_CUT_MEAN:
            cut = True
            diffs.append(np.zeros_like(d))
            continue
        d[:30, :] = 0
        d[int(270 * 0.83):, :] = 0
        d[:, :45] = 0
        d[d < 25] = 0
        acc += d
        diffs.append(d)
    ys, xs = np.where(acc > max(acc.max() * 0.25, 20))
    cx, cy = (int(np.median(xs)) * fw // 480, int(np.median(ys)) * fh // 270) if len(xs) > 20 else (fw // 2, fh // 2)
    cw, ch = fw * 5 // 12, fh * 5 // 12
    x0, y0 = min(max(cx - cw // 2, 0), fw - cw), min(max(cy - ch // 2, 0), fh - ch)
    rx0, ry0, rx1, ry1 = x0 * 480 // fw, y0 * 270 // fh, (x0 + cw) * 480 // fw, (y0 + ch) * 270 // fh
    motion = [0.0] + [float(d[ry0:ry1, rx0:rx1].mean()) for d in diffs[1:]]
    allmotion += motion[1:]
    plans.append((t, times, imgs, motion, (x0, y0, cw, ch)))
floor = float(np.percentile(allmotion, 30)) if allmotion else 0.0
for t, times, imgs, motion, (x0, y0, cw, ch) in plans:
    peak = max(motion)
    tail = motion[int(len(motion) * 0.75):]
    lingering = peak > floor * 4 + 0.5 and tail and max(tail) > max(peak * 0.5, floor * 4)
    if lingering:
        report.append(f"{t['master']} {t['slot']} {t['skill']} {t['caster']} {t['test']}: motion still {max(tail):.2f} (peak {peak:.2f}, floor {floor:.2f}) at the end of the window")
    cells = []
    for k, im in enumerate(imgs):
        c = cv2.resize(im[y0:y0 + ch, x0:x0 + cw], (W, H))
        cv2.putText(c, f'+{(times[k] - t["wall"]) / 1000:.2f}', (4, 16), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 255, 255), 1)
        cv2.rectangle(c, (0, H - 6), (int(W * min(motion[k] / max(peak, 1e-3), 1)), H), (0, 200, 0), -1)
        cells.append(c)
    rows = [np.hstack(cells[r:r + COLS] + [np.zeros((H, W, 3), np.uint8)] * (COLS - len(cells[r:r + COLS]))) for r in range(0, len(cells), COLS)]
    head = f"{t['slot']} {t['skill']} caster={t['caster']} test={t['test']} dist={t['dist']}{t['note']}" + ('  LINGERING MOTION' if lingering else '')
    by_master.setdefault(t['master'], []).append(np.vstack([label(head, W * COLS, (0, 0, 255) if lingering or 'STUCK' in head else (0, 255, 255))] + rows))

for m, blocks in by_master.items():
    for n, block in enumerate(blocks):
        cv2.imwrite(os.path.join(out, f'master_{m:02d}_{n:02d}.jpg'), block, [cv2.IMWRITE_JPEG_QUALITY, 75])
with open(os.path.join(out, 'report.txt'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(report) + '\n')
print(f'{len(tests)} tests, {sum(len(b) for b in by_master.values())} rows, {len(report)} flagged')
