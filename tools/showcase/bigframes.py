import cv2, numpy as np, glob, os, re, sys

args = [a for a in sys.argv[1:] if not a.startswith('--crop=')]
crop = next((a[7:] for a in sys.argv[1:] if a.startswith('--crop=')), None)
capdir, log, skill, test, out = args[:5]
offsets = [float(x) for x in (args[5] if len(args) > 5 else '0.3,0.7,1.1,1.6').split(',')]
frames = sorted((int(os.path.splitext(os.path.basename(f))[0]), f) for f in glob.glob(os.path.join(capdir, '*.jpg')))
wall = None
for line in open(log, encoding='utf-8', errors='replace'):
    m = re.search(r'Showcase :: wall=(\d+) .*skill=(\d+) .*test=(\S+)', line)
    if m and m.group(2) == skill and m.group(3) == test:
        wall = int(m.group(1))
        break
if wall is None:
    sys.exit('test not found')
cells = []
for off in offsets:
    t = wall + int(off * 1000)
    best = min(frames, key=lambda f: abs(f[0] - t))
    img = cv2.imread(best[1])
    if crop:
        x0, y0, x1, y1 = (float(v) for v in crop.split(','))
        h, w = img.shape[:2]
        img = img[int(h * y0):int(h * y1), int(w * x0):int(w * x1)]
    img = cv2.resize(img, (1440, 810), interpolation=cv2.INTER_CUBIC)
    cv2.putText(img, f'{(best[0] - wall) / 1000:+.2f}s', (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 255, 255), 3)
    cells.append(img)
while len(cells) < 4:
    cells.append(np.zeros((810, 1440, 3), np.uint8))
cv2.imwrite(out, np.vstack([np.hstack(cells[:2]), np.hstack(cells[2:4])]), [cv2.IMWRITE_JPEG_QUALITY, 80])
print('ok', out)
