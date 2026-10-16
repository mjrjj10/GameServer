import cv2, glob, os, re, sys
import numpy as np

capdir, log, skill, test, out = sys.argv[1:6]
ours = [float(x) for x in (sys.argv[6] if len(sys.argv) > 6 else '0.2,0.6,1.0').split(',')]
crop = [float(x) for x in (sys.argv[7] if len(sys.argv) > 7 else '0.2,0.15,0.8,0.65').split(',')]
ref = glob.glob(f'D:/MxM/showcase_data/reference/*/{skill}')
W, H = 960, 540


def label(img, text, color):
    cv2.putText(img, text, (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.1, color, 3)
    return img


top = []
if ref:
    fr = sorted(glob.glob(os.path.join(ref[0], 'frame_*.jpg')))
    if fr:
        for k in np.linspace(len(fr) * 0.15, len(fr) * 0.75, 3).astype(int):
            name = os.path.basename(fr[k])[6:-5]
            top.append(label(cv2.resize(cv2.imread(fr[k]), (W, H)), 'preview ' + name, (0, 255, 0)))
while len(top) < 3:
    top.append(np.zeros((H, W, 3), np.uint8))

frames = sorted((int(os.path.splitext(os.path.basename(f))[0]), f) for f in glob.glob(os.path.join(capdir, '*.jpg')))
wall = None
for line in open(log, encoding='utf-8', errors='replace'):
    m = re.search(r'Showcase :: wall=(\d+) .*skill=(\d+) .*test=(\S+)', line)
    if m and m.group(2) == skill and m.group(3) == test:
        wall = int(m.group(1))
        break
bottom = []
if wall is not None and frames:
    for off in ours:
        best = min(frames, key=lambda f: abs(f[0] - wall - off * 1000))
        img = cv2.imread(best[1])
        h, w = img.shape[:2]
        img = img[int(h * crop[1]):int(h * crop[3]), int(w * crop[0]):int(w * crop[2])]
        bottom.append(label(cv2.resize(img, (W, H), interpolation=cv2.INTER_CUBIC), f'ours {(best[0] - wall) / 1000:+.2f}s', (0, 255, 255)))
while len(bottom) < 3:
    bottom.append(np.zeros((H, W, 3), np.uint8))
cv2.imwrite(out, np.vstack([np.hstack(top), np.hstack(bottom)]), [cv2.IMWRITE_JPEG_QUALITY, 75])
print('ok', out)
