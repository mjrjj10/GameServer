import os
import sys
import time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive


def main():
    pid, logpath, outpath = int(sys.argv[1]), sys.argv[2], sys.argv[3]
    limit = float(sys.argv[4]) if len(sys.argv) > 4 else 900
    out = open(outpath, 'w', encoding='utf-8')
    out.write('gx agent start pid=%d\n' % pid)
    out.flush()
    t0 = time.time()
    f = open(logpath, encoding='utf-8', errors='replace')
    while time.time() - t0 < limit:
        line = f.readline()
        if not line:
            time.sleep(0.05)
            continue
        if 'GxTest :: press F' in line:
            drive.focus(pid)
            time.sleep(0.3)
            for attempt in range(6):
                drive.key('f', False)
                time.sleep(0.08)
                drive.key('f', True)
                time.sleep(0.25)
            out.write('pressed F t=%d\n' % int(time.time() * 1000))
            out.flush()
        if 'Showcase :: done' in line:
            break
    out.write('gx agent end\n')
    out.close()


if __name__ == '__main__':
    main()
