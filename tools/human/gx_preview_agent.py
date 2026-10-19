import os
import re
import sys
import time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
import mirror_agent as ma
import math

PROMPT = re.compile(r'Preview :: prompt key=(\S+) click=(\d) hold=(\d+) count=(\d+) aim=\(([-\d.]+), ([-\d.]+)\)')
HUMAN_SCREEN = (960, 575)
WORLD_TO_SCREEN_H = ((0.432787797, -0.368195785, 3.85857082), (-0.255139072, -0.265550233, -12.8246199), (-0.000100763313, -0.0000256688599, 1.0))
ENTITY_CLICK_RAISE = 40
ENEMY_BODY_RAISE = 60
ENEMY = re.compile(r'enemy=1')
DRIVE_KEYS = ('w', 'd', 's', 'a')


def world_to_screen(ax, ay):
    h = WORLD_TO_SCREEN_H
    w = h[2][0] * ax + h[2][1] * ay + h[2][2]
    return (HUMAN_SCREEN[0] + (h[0][0] * ax + h[0][1] * ay + h[0][2]) / w, HUMAN_SCREEN[1] + (h[1][0] * ax + h[1][1] * ay + h[1][2]) / w)


def click_left():
    drive.mouse(drive.MOUSEEVENTF_LEFTDOWN)
    time.sleep(0.05)
    drive.mouse(drive.MOUSEEVENTF_LEFTUP)


def tap(k):
    drive.key(k, False)
    time.sleep(0.06)
    drive.key(k, True)


def enemy_point(ax, ay, f, out, key):
    tgt = ma.find_enemy(ma.grab())
    if not tgt:
        out.write('enemy not found on screen' + chr(10))
        return world_to_screen(ax, ay)
    x, y = tgt[0], tgt[1] - ENEMY_BODY_RAISE
    if key in ('lmb', 'lmbhold'):
        drive.move(x, y)
        time.sleep(0.15)
        x, y = ma.aim_loop(f, out, x, y, math.atan2(ay, ax))
    return x, y


def act(m, out, f):
    key, click, hold, count = m.group(1), m.group(2) == '1', int(m.group(3)), int(m.group(4))
    ax, ay = float(m.group(5)), float(m.group(6))
    if ENEMY.search(m.string):
        x, y = enemy_point(ax, ay, f, out, key)
    else:
        x, y = world_to_screen(ax, ay)
        if key == 'r':
            y -= ENTITY_CLICK_RAISE
    drive.move(x, y)
    time.sleep(0.25)
    if key == 'lmb':
        for i in range(count):
            click_left()
            time.sleep(0.45)
    elif key == 'lmbhold':
        drive.mouse(drive.MOUSEEVENTF_LEFTDOWN)
        time.sleep(hold / 1000.0)
        drive.mouse(drive.MOUSEEVENTF_LEFTUP)
    elif key == 'rmb':
        drive.mouse(drive.MOUSEEVENTF_RIGHTDOWN)
        time.sleep(0.05)
        drive.mouse(drive.MOUSEEVENTF_RIGHTUP)
    elif key == 'drive':
        for k in DRIVE_KEYS:
            drive.key(k, False)
            time.sleep(hold / 1000.0)
            drive.key(k, True)
    else:
        tap(key)
        if click:
            time.sleep(0.35)
            drive.move(x, y)
            click_left()
    out.write('acted key=%s at=(%.0f,%.0f) t=%d\n' % (key, x, y, int(time.time() * 1000)))
    out.flush()


def main():
    pid, logpath, outpath = int(sys.argv[1]), sys.argv[2], sys.argv[3]
    limit = float(sys.argv[4]) if len(sys.argv) > 4 else 900
    out = open(outpath, 'w', encoding='utf-8')
    t0 = time.time()
    f = open(logpath, encoding='utf-8', errors='replace')
    while time.time() - t0 < limit:
        line = f.readline()
        if not line:
            time.sleep(0.05)
            continue
        m = PROMPT.search(line)
        if m:
            drive.focus(pid)
            time.sleep(0.2)
            act(m, out, f)
        if 'Showcase :: done' in line:
            break
    out.write('preview agent end\n')
    out.close()


if __name__ == '__main__':
    main()
