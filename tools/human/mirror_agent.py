import os, re, sys, time
import numpy as np, cv2, mss
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

PROMPT = re.compile(r'Mirror :: prompt wall=(\d+) key=(\S+) click=(\d) combo=(\d) skill=(\d+) test=(\S+) dist=(\d+) aim=\((-?\d+), (-?\d+)\)')
HUMAN_SCREEN = (960, 575)
PX_PER_UNIT = (-0.577, -0.047)


def grab():
    with mss.mss() as s:
        mon = next((m for m in s.monitors[1:] if m['left'] == 0 and m['top'] == 0), s.monitors[1])
        return cv2.cvtColor(np.array(s.grab(mon)), cv2.COLOR_BGRA2BGR)


def find_enemy(img, near=None):
    h, w = img.shape[:2]
    b, g, r = img[:, :, 0].astype(int), img[:, :, 1].astype(int), img[:, :, 2].astype(int)
    mask = ((r > 120) & (r > g * 2.5) & (r > b * 2.5)).astype(np.uint8)
    mask = cv2.dilate(mask, np.ones((1, 9), np.uint8))
    mask[int(h * 0.85):, :] = 0
    mask[:int(h * 0.08), :] = 0
    n, lab, stats, cent = cv2.connectedComponentsWithStats(mask)
    best = None
    for i in range(1, n):
        x, y, bw, bh, area = stats[i]
        if bw < 100 or bw > 170 or bh > 14 or area < 300:
            continue
        cx, cy = x + bw / 2, y + bh / 2
        ref = near if near else (w / 2, h / 2)
        d = (cx - ref[0]) ** 2 + (cy + 115 - ref[1]) ** 2
        if near and d > 200 ** 2:
            continue
        if best is None or d < best[0]:
            best = (d, cx, cy + 115)
    return (best[1], best[2]) if best else None


def act(m, pid, out, f, dbgdir):
    key, click, combo, test = m.group(2), m.group(3) == '1', m.group(4) == '1', m.group(6)
    drive.focus(pid)
    time.sleep(1.0)
    ax, ay = float(m.group(8)), float(m.group(9))
    along = (ay - ax) * 0.7071
    expect = (HUMAN_SCREEN[0] + along * PX_PER_UNIT[0], HUMAN_SCREEN[1] + along * PX_PER_UNIT[1])
    tgt = None
    for attempt in range(2):
        img = grab()
        tgt = find_enemy(img, expect)
        if tgt:
            break
        time.sleep(0.3)
    if not tgt:
        cv2.imwrite(os.path.join(dbgdir, f'none_{m.group(1)}.jpg'), cv2.resize(img, (960, 540)))
    x, y = tgt if tgt else expect
    if key == 'lmb':
        y -= 55
    dx, dy = x - HUMAN_SCREEN[0], y - HUMAN_SCREEN[1]
    k = 1.0
    for lim, v in ((HUMAN_SCREEN[0] - 60, -dx), (1920 - 60 - HUMAN_SCREEN[0], dx), (HUMAN_SCREEN[1] - 120, -dy), (1080 - 200 - HUMAN_SCREEN[1], dy)):
        if v > lim:
            k = min(k, lim / v)
    x, y = HUMAN_SCREEN[0] + dx * k, HUMAN_SCREEN[1] + dy * k
    out.write(f'prompt {m.group(1)} key={key} skill={m.group(5)} test={test} target={"found" if tgt else "none"} at=({x:.0f},{y:.0f}) t={int(time.time() * 1000)}\n')
    out.flush()
    drive.move(x, y)
    time.sleep(0.15)
    if key == 'none':
        return
    if key == 'lmb':
        drive.mouse(drive.MOUSEEVENTF_LEFTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_LEFTUP)
    elif key == 'rmb':
        drive.mouse(drive.MOUSEEVENTF_RIGHTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_RIGHTUP)
    elif key == 'wheel':
        i = drive.INPUT(type=drive.INPUT_MOUSE)
        i.u.mi = drive.MOUSEINPUT(0, 0, (-120) & 0xFFFFFFFF, 0x0800, 0, 0)
        drive.send(i)
    else:
        drive.key(key, False); time.sleep(0.05); drive.key(key, True)
        if click:
            time.sleep(0.25)
            drive.mouse(drive.MOUSEEVENTF_LEFTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_LEFTUP)
        if combo:
            time.sleep(0.7)
            drive.key(key, False); time.sleep(0.05); drive.key(key, True)
            if click:
                time.sleep(0.25)
                drive.mouse(drive.MOUSEEVENTF_LEFTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_LEFTUP)
    skill = m.group(5)
    if skill != '0' and key in ('q', 'e', 'r'):
        seen = False
        end = time.time() + 1.5
        while time.time() < end and not seen:
            line = f.readline()
            if not line:
                time.sleep(0.05)
                continue
            if f'skill={skill}' in line and 'Skill :: ' in line:
                seen = True
        if not seen:
            drive.key(key, False); time.sleep(0.05); drive.key(key, True)
            out.write(f'cast not seen, closed targeting {m.group(1)}\n')
    out.write(f'done {m.group(1)} t={int(time.time() * 1000)}\n')
    out.flush()


def main():
    pid, logpath, outpath = int(sys.argv[1]), sys.argv[2], sys.argv[3]
    limit = float(sys.argv[4]) if len(sys.argv) > 4 else 3600
    end = time.time() + limit
    with open(outpath, 'a') as out, open(logpath, 'r', encoding='utf-8', errors='replace') as f:
        out.write(f'agent start pid={pid} t={int(time.time() * 1000)}\n')
        f.seek(0, 2)
        while time.time() < end:
            line = f.readline()
            if not line:
                time.sleep(0.05)
                continue
            if 'Showcase :: done' in line:
                out.write('showcase done\n')
                break
            m = PROMPT.search(line)
            if m:
                act(m, pid, out, f, os.path.dirname(outpath))


if __name__ == '__main__':
    main()
