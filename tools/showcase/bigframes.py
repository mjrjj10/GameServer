import cv2, numpy as np, glob, os, re, sys
capdir, log, skill, test, out = sys.argv[1:6]
offsets = [float(x) for x in (sys.argv[6] if len(sys.argv) > 6 else '0.3,0.7,1.1,1.6').split(',')]
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
    img = cv2.resize(cv2.imread(best[1]), (1440, 810))
    cv2.putText(img, f'+{(best[0] - wall) / 1000:.1f}s', (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 255, 255), 3)
    cells.append(img)
while len(cells) < 4:
    cells.append(np.zeros((810, 1440, 3), np.uint8))
cv2.imwrite(out, np.vstack([np.hstack(cells[:2]), np.hstack(cells[2:4])]), [cv2.IMWRITE_JPEG_QUALITY, 80])
print('ok', out)
