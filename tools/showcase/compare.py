import cv2, numpy as np, glob, os, re, sys
previews, out = sys.argv[1], sys.argv[2]
sheetdirs = sys.argv[3:]
os.makedirs(out, exist_ok=True)
COLS, W, H = 6, 480, 270

def preview_row(path):
    cap = cv2.VideoCapture(path)
    n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT)); fps = cap.get(cv2.CAP_PROP_FPS) or 30
    if n <= 0: return None
    cells = []
    for k in np.linspace(n * 0.05, n * 0.95, COLS * 2).astype(int):
        cap.set(cv2.CAP_PROP_POS_FRAMES, int(k)); ok, fr = cap.read()
        if not ok: continue
        c = cv2.resize(fr, (W, H)); cv2.putText(c, f'{k / fps:.1f}s', (6, 26), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2); cells.append(c)
    while len(cells) < COLS * 2: cells.append(np.zeros((H, W, 3), np.uint8))
    return np.vstack([np.hstack(cells[:COLS]), np.hstack(cells[COLS:])])

done = 0
for d in sheetdirs:
    for sheet in sorted(glob.glob(os.path.join(d, 'm*_*.jpg'))):
        img = cv2.imread(sheet)
        m = re.match(r'm(\d+)_(\w+)\.jpg', os.path.basename(sheet))
        txt = sheet + '.skill'
        skill = open(txt).read().strip() if os.path.exists(txt) else None
        if not skill: continue
        pv = os.path.join(previews, f'{skill}.mp4')
        if not os.path.exists(pv): continue
        row = preview_row(pv)
        if row is None: continue
        label = np.zeros((34, row.shape[1], 3), np.uint8)
        cv2.putText(label, f'PREVIEW OFFICIELLE {skill}', (8, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        if img.shape[1] != row.shape[1]: img = cv2.resize(img, (row.shape[1], int(img.shape[0] * row.shape[1] / img.shape[1])))
        cv2.imwrite(os.path.join(out, f'm{int(m.group(1)):02d}_{m.group(2)}_{skill}.jpg'), np.vstack([label, row, img]), [cv2.IMWRITE_JPEG_QUALITY, 70])
        done += 1
print('compared', done)
