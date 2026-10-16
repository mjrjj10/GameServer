import cv2, numpy as np, sys
v, out, step, t0, t1 = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
cap = cv2.VideoCapture(v)
cells = []
t = t0
while t <= t1:
    cap.set(cv2.CAP_PROP_POS_MSEC, t * 1000)
    ok, f = cap.read()
    if not ok: break
    f = cv2.resize(f, (480, 270))
    cv2.putText(f, f'{t:.1f}', (8, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 255), 2)
    cells.append(f); t += step
cols = 5
while len(cells) % cols: cells.append(np.zeros((270, 480, 3), np.uint8))
rows = [np.hstack(cells[i:i + cols]) for i in range(0, len(cells), cols)]
cv2.imwrite(out, np.vstack(rows), [cv2.IMWRITE_JPEG_QUALITY, 70])
print('ok', out, len(cells))
