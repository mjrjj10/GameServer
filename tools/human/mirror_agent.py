import math, os, re, sys, time
import numpy as np, cv2, mss
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

PROMPT = re.compile(r'Mirror :: prompt wall=(\d+) key=(\S+) click=(\d) combo=(\d) skill=(\d+) test=(\S+) dist=(\d+) aim=\((-?\d+), (-?\d+)\)(?: dironly=(\d))?(?: posdir=(\d))?(?: comboclick=(\d))?(?: combotele=(\d))?')
HUMAN_SCREEN = (960, 575)
WORLD_X_PX = (0.390, -0.253)
WORLD_Y_PX = (-0.379, -0.286)


def world_to_screen(ax, ay):
    return (HUMAN_SCREEN[0] + ax * WORLD_X_PX[0] + ay * WORLD_Y_PX[0], HUMAN_SCREEN[1] + ax * WORLD_X_PX[1] + ay * WORLD_Y_PX[1])


def grab():
    with mss.mss() as s:
        mon = next((m for m in s.monitors[1:] if m['left'] == 0 and m['top'] == 0), s.monitors[1])
        return cv2.cvtColor(np.array(s.grab(mon)), cv2.COLOR_BGRA2BGR)


def find_enemy(img, near=None, minw=100):
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
        if bw < minw or bw > 170 or bh > 14 or area < min(300, minw * 4):
            continue
        cx, cy = (x + bw / 2 if bw >= 100 else x + 60), y + bh / 2
        ref = near if near else (w / 2, h / 2)
        d = (cx - ref[0]) ** 2 + (cy + 115 - ref[1]) ** 2
        if near and d > 200 ** 2:
            continue
        if best is None or d < best[0]:
            best = (d, cx, cy + 115)
    return (best[1], best[2]) if best else None


LAST_YAW = [None]


def read_yaw(f):
    pos = f.tell()
    data = f.read()
    vals = re.findall(r'Aim :: yaw=(-?[0-9.]+)', data)
    if vals:
        LAST_YAW[0] = float(vals[-1])
    f.seek(pos)
    return LAST_YAW[0]


def wrap(a):
    while a > math.pi: a -= 2 * math.pi
    while a < -math.pi: a += 2 * math.pi
    return a


def aim_loop(f, out, x, y, target):
    r = max(math.hypot(x - HUMAN_SCREEN[0], y - HUMAN_SCREEN[1]), 120.0)
    s = math.atan2(y - HUMAN_SCREEN[1], x - HUMAN_SCREEN[0])
    prev = None
    k = -1.0
    for it in range(7):
        px, py = HUMAN_SCREEN[0] + r * math.cos(s), HUMAN_SCREEN[1] + r * math.sin(s)
        drive.move(px, py)
        time.sleep(0.4)
        yaw = read_yaw(f)
        if yaw is None:
            return px, py
        err = wrap(target - yaw)
        if prev is not None and abs(s - prev[0]) > 1e-3:
            dk = wrap(yaw - prev[1]) / (s - prev[0])
            if abs(dk) > 0.2: k = dk
        out.write(f'aim it={it} screen={math.degrees(s):.1f} yaw={math.degrees(yaw):.1f} target={math.degrees(target):.1f}'+chr(10))
        if abs(err) < math.radians(5):
            return px, py
        prev = (s, yaw)
        s = s + err / k
    return px, py


def act(m, pid, out, f, dbgdir, follow='none'):
    key, click, combo, test = m.group(2), m.group(3) == '1', m.group(4) == '1', m.group(6)
    drive.focus(pid)
    time.sleep(1.0)
    ax, ay = float(m.group(8)), float(m.group(9))
    expect = world_to_screen(ax, ay)
    tgt = None
    for attempt in range(2):
        img = grab()
        tgt = find_enemy(img, expect) or find_enemy(img, expect, 30)
        if tgt:
            break
        time.sleep(0.3)
    if not tgt:
        cv2.imwrite(os.path.join(dbgdir, f'none_{m.group(1)}.jpg'), cv2.resize(img, (960, 540)))
    x, y = tgt if tgt else expect
    corrected = False
    if tgt:
        ex, ey = expect[0] - HUMAN_SCREEN[0], expect[1] - HUMAN_SCREEN[1]
        tx, ty = x - HUMAN_SCREEN[0], y - HUMAN_SCREEN[1]
        el, tl = math.hypot(ex, ey), math.hypot(tx, ty)
        if el > 1 and tl > 1:
            cosang = (ex * tx + ey * ty) / (el * tl)
            if cosang < math.cos(math.radians(10)):
                x, y = HUMAN_SCREEN[0] + ex / el * tl, HUMAN_SCREEN[1] + ey / el * tl
                corrected = True
                out.write(f'aim corrected to expected direction ({x:.0f},{y:.0f})'+chr(10))
    if key in ('q', 'e', 'r') and click and m.group(10) != '1':
        x, y = expect
        out.write(f'ground click on computed point ({x:.0f},{y:.0f})'+chr(10))
    if key == 'lmb' and not corrected:
        y -= 55
    dx, dy = x - HUMAN_SCREEN[0], y - HUMAN_SCREEN[1]
    k = 1.0
    for lim, v in ((HUMAN_SCREEN[0] - 60, -dx), (1920 - 60 - HUMAN_SCREEN[0], dx), (HUMAN_SCREEN[1] - 120, -dy), (1080 - 200 - HUMAN_SCREEN[1], dy)):
        if v > lim:
            k = min(k, lim / v)
    x, y = HUMAN_SCREEN[0] + dx * k, HUMAN_SCREEN[1] + dy * k
    if m.group(10) == '1':
        if math.hypot(expect[0] - HUMAN_SCREEN[0], expect[1] - HUMAN_SCREEN[1]) < 150:
            x, y = expect
        d = math.hypot(x - HUMAN_SCREEN[0], y - HUMAN_SCREEN[1])
        if 1 < d < 350:
            x, y = HUMAN_SCREEN[0] + (x - HUMAN_SCREEN[0]) * 350 / d, HUMAN_SCREEN[1] + (y - HUMAN_SCREEN[1]) * 350 / d
    out.write(f'prompt {m.group(1)} key={key} skill={m.group(5)} test={test} target={"found" if tgt else "none"} at=({x:.0f},{y:.0f}) t={int(time.time() * 1000)}\n')
    out.flush()
    drive.move(x, y)
    time.sleep(0.15)
    if key == 'lmb' or m.group(10) == '1' or key in ('q', 'e', 'r'):
        x0, y0 = x, y
        x, y = aim_loop(f, out, x, y, math.atan2(float(m.group(9)), float(m.group(8))))
        if key in ('q', 'e', 'r') and click:
            x, y = x0, y0
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
        mark = f.tell()
        drive.key(key, False); time.sleep(0.05); drive.key(key, True)
        if click:
            time.sleep(0.25)
            drive.mouse(drive.MOUSEEVENTF_LEFTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_LEFTUP)
            if m.group(11) == '1':
                time.sleep(0.25)
                vx, vy = x - HUMAN_SCREEN[0], y - HUMAN_SCREEN[1]
                vl = max(math.hypot(vx, vy), 1.0)
                drive.move(x + vx / vl * 150, y + vy / vl * 150)
                time.sleep(0.15)
                drive.mouse(drive.MOUSEEVENTF_LEFTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_LEFTUP)
                out.write('posdir second click'+chr(10))
        skill_id = m.group(5)
        first_seen = False
        end = time.time() + 1.5
        while time.time() < end and not first_seen:
            time.sleep(0.1)
            f.seek(mark)
            first_seen = f'Skill :: alexis skill={skill_id}' in f.read()
        if not first_seen and skill_id != '0':
            drive.key(key, False); time.sleep(0.05); drive.key(key, True)
            out.write(f'cast not seen, closed targeting {m.group(1)}'+chr(10))
        elif combo and m.group(12) == '0' and m.group(13) != '1':
            time.sleep(0.7)
            mark = f.tell()
            got = False
            for attempt in range(10):
                drive.key(key, False); time.sleep(0.05); drive.key(key, True)
                time.sleep(0.25)
                f.seek(mark)
                if 'Skill :: alexis' in f.read():
                    got = True
                    break
            out.write(f'fast combo {"seen" if got else "not seen"} after {attempt + 1} presses'+chr(10))
        elif combo:
            time.sleep(2.0)
            again = (x, y) if m.group(13) == '1' else find_enemy(grab(), None, 30)
            if again:
                drive.move(again[0], again[1])
                time.sleep(0.15)
            out.write(f'combo aim {again}'+chr(10))
            mark = f.tell()
            drive.key(key, False); time.sleep(0.05); drive.key(key, True)
            if m.group(12) == '1' or (m.group(12) is None and click):
                time.sleep(0.25)
                drive.mouse(drive.MOUSEEVENTF_LEFTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_LEFTUP)
            time.sleep(1.0)
            f.seek(mark)
            if 'Skill :: alexis' not in f.read():
                drive.key(key, False); time.sleep(0.05); drive.key(key, True)
                out.write('combo not seen, closed targeting'+chr(10))
        f.seek(0, 2)
    if follow in ('q', 'e'):
        time.sleep(1.0)
        drive.key(follow, False); time.sleep(0.05); drive.key(follow, True)
        time.sleep(0.25)
        drive.mouse(drive.MOUSEEVENTF_LEFTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_LEFTUP)
        out.write(f'follow {follow}'+chr(10))
    skill = m.group(5)
    if skill != '0' and key == 'rmb':
        seen = False
        end = time.time() + 1.2
        while time.time() < end and not seen:
            line = f.readline()
            if not line:
                time.sleep(0.05)
                continue
            seen = f'skill={skill}' in line and 'Skill :: ' in line
        if not seen:
            drive.move(x, y)
            time.sleep(0.1)
            drive.mouse(drive.MOUSEEVENTF_RIGHTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_RIGHTUP)
            out.write('dodge retried'+chr(10))
    out.write(f'done {m.group(1)} t={int(time.time() * 1000)}\n')
    out.flush()


def main():
    probed = False
    pid, logpath, outpath = int(sys.argv[1]), sys.argv[2], sys.argv[3]
    limit = float(sys.argv[4]) if len(sys.argv) > 4 else 3600
    end = time.time() + limit
    with open(outpath, 'a') as out, open(logpath, 'r', encoding='utf-8', errors='replace') as f:
        out.write(f'agent start pid={pid} t={int(time.time() * 1000)}\n')
        f.seek(0, 2)
        follow = ['none']
        while time.time() < end:
            line = f.readline()
            if not line:
                time.sleep(0.05)
                continue
            if 'Showcase :: done' in line:
                out.write('showcase done\n')
                break
            m = PROMPT.search(line)
            fm = re.search(r'Mirror :: follow=(\S+)', line)
            if fm:
                follow[0] = fm.group(1)
            if m:
                if os.path.exists(r'D:/MxM/showcase_data/runs/probe_dodge.flag') and not probed:
                    probed = True
                    drive.focus(pid); time.sleep(1.0)
                    drive.move(HUMAN_SCREEN[0] + 200, HUMAN_SCREEN[1] + 100); time.sleep(0.15)
                    mark = f.tell()
                    drive.mouse(drive.MOUSEEVENTF_RIGHTDOWN); time.sleep(0.05); drive.mouse(drive.MOUSEEVENTF_RIGHTUP)
                    time.sleep(1.5); f.seek(mark)
                    out.write('probe dodge ' + ('seen' if 'Skill :: alexis' in f.read() else 'not seen') + chr(10))
                act(m, pid, out, f, os.path.dirname(outpath), follow[0])
                follow[0] = 'none'


if __name__ == '__main__':
    main()
