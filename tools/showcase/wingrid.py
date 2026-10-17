import ctypes, ctypes.wintypes as wt, os, sys, time, threading
import numpy as np, cv2
from windows_capture import WindowsCapture, Frame, InternalCaptureControl

user32 = ctypes.windll.user32
EnumProc = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)


def windows_of(pid):
    found = []

    def cb(hwnd, _):
        p = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(hwnd):
            r = wt.RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(r))
            if r.right - r.left > 300:
                found.append(hwnd)
        return True
    user32.EnumWindows(EnumProc(cb), 0)
    return found


def client_rect(hwnd):
    r = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    pt = wt.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return pt.x, pt.y, r.right - r.left, r.bottom - r.top


def start_captures(pids):
    latest = {}
    lock = threading.Lock()
    caps = []
    for i, pid in enumerate(pids):
        hwnd = None
        for _ in range(90):
            ws = windows_of(pid)
            if ws:
                hwnd = ws[0]
                break
            time.sleep(1)
        if not hwnd:
            print(f'no window for pid {pid}')
            continue
        cap = WindowsCapture(cursor_capture=False, draw_border=False, window_hwnd=hwnd)

        def on_frame_arrived(frame: Frame, control: InternalCaptureControl, i=i):
            with lock:
                latest[i] = frame.frame_buffer[:, :, :3].copy()

        def on_closed():
            pass
        cap.event(on_frame_arrived)
        cap.event(on_closed)
        cap.start_free_threaded()
        caps.append(cap)
        print(f'pid {pid} -> window {hwnd} capturing')
    return latest, lock, caps


def capture(pids, outdirs, seconds, interval):
    for d in outdirs:
        os.makedirs(d, exist_ok=True)
    latest, lock, caps = start_captures(pids)
    end = time.time() + seconds
    while time.time() < end:
        t = int(time.time() * 1000)
        with lock:
            frames = dict(latest)
        for i, img in frames.items():
            cv2.imwrite(os.path.join(outdirs[i], f'{t}.jpg'), img, [cv2.IMWRITE_JPEG_QUALITY, 80])
        time.sleep(max(0.0, interval - (time.time() * 1000 - t) / 1000))


if __name__ == '__main__':
    pids = [int(p) for p in sys.argv[1].split(',')]
    base = sys.argv[2]
    seconds = float(sys.argv[3])
    capture(pids, [f'{base}_g{i}' for i in range(len(pids))], seconds, 0.4)
