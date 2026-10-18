import cv2, glob, os, sys
import numpy as np

root = sys.argv[1]
out = os.path.join(root, 'official')
W, H, COLS = 400, 225, 8


def sheet(frames, stamps, path, title):
    cells = []
    for im, t in zip(frames, stamps):
        c = cv2.resize(im, (W, H))
        cv2.putText(c, f'{t:.2f}', (4, 16), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 255, 255), 1)
        cells.append(c)
    rows = [np.hstack(cells[r:r + COLS] + [np.zeros((H, W, 3), np.uint8)] * (COLS - len(cells[r:r + COLS]))) for r in range(0, len(cells), COLS)]
    head = np.zeros((24, W * COLS, 3), np.uint8)
    cv2.putText(head, title, (6, 17), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 200, 0), 1)
    cv2.imwrite(path, np.vstack([head] + rows), [cv2.IMWRITE_JPEG_QUALITY, 75])


def extract(video, step, frame_dir, sheet_dir, name, sheet_span):
    cap = cv2.VideoCapture(video)
    fps = cap.get(cv2.CAP_PROP_FPS) or 30
    n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    if n <= 0:
        return 0
    os.makedirs(frame_dir, exist_ok=True)
    os.makedirs(sheet_dir, exist_ok=True)
    dur = n / fps
    t, chunk, chunk_t, part, count = 0.0, [], [], 0, 0
    pos = 0.0
    while t < dur:
        ok, f = cap.read()
        if not ok:
            break
        pos = cap.get(cv2.CAP_PROP_POS_MSEC) / 1000.0
        if pos + 0.5 / fps < t:
            continue
        cv2.imwrite(os.path.join(frame_dir, f'{int(pos * 1000):08d}.jpg'), f, [cv2.IMWRITE_JPEG_QUALITY, 90])
        chunk.append(cv2.resize(f, (W, H)))
        chunk_t.append(pos)
        count += 1
        if chunk_t[-1] - chunk_t[0] >= sheet_span:
            sheet(chunk, chunk_t, os.path.join(sheet_dir, f'{name}_{part:03d}.jpg'), f'{name} {chunk_t[0]:.1f}-{chunk_t[-1]:.1f}s')
            part += 1
            chunk, chunk_t = [], []
        t = pos + step
    if chunk:
        sheet(chunk, chunk_t, os.path.join(sheet_dir, f'{name}_{part:03d}.jpg'), f'{name} {chunk_t[0]:.1f}-{chunk_t[-1]:.1f}s')
    return count


jobs = [('previews', 1.0 / 30.0, 1.6), ('masters', 0.1, 4.8), ('matches', 0.25, 12.0)]
for kind, step, span in jobs:
    for v in sorted(glob.glob(os.path.join(root, kind, '*.mp4'))):
        name = os.path.splitext(os.path.basename(v))[0]
        done = os.path.join(out, kind, 'sheets', f'{name}_000.jpg')
        if os.path.exists(done):
            continue
        n = extract(v, step, os.path.join(out, kind, 'frames', name), os.path.join(out, kind, 'sheets'), name, span)
        print(kind, name, n, flush=True)
print('official base done', flush=True)
