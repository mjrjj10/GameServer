import ctypes, ctypes.wintypes as w, sys, time, subprocess

user32 = ctypes.WinDLL('user32', use_last_error=True)
INPUT_MOUSE, INPUT_KEYBOARD = 0, 1
MOUSEEVENTF_MOVE, MOUSEEVENTF_ABSOLUTE = 0x0001, 0x8000
MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP = 0x0002, 0x0004
MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP = 0x0008, 0x0010
KEYEVENTF_KEYUP, KEYEVENTF_SCANCODE = 0x0002, 0x0008


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [('dx', w.LONG), ('dy', w.LONG), ('mouseData', w.DWORD), ('dwFlags', w.DWORD), ('time', w.DWORD), ('dwExtraInfo', ctypes.c_size_t)]


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [('wVk', w.WORD), ('wScan', w.WORD), ('dwFlags', w.DWORD), ('time', w.DWORD), ('dwExtraInfo', ctypes.c_size_t)]


class _U(ctypes.Union):
    _fields_ = [('mi', MOUSEINPUT), ('ki', KEYBDINPUT), ('pad', ctypes.c_byte * 32)]


class INPUT(ctypes.Structure):
    _fields_ = [('type', w.DWORD), ('u', _U)]


def send(inp):
    user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))


def mouse(flags, x=0, y=0):
    i = INPUT(type=INPUT_MOUSE)
    i.u.mi = MOUSEINPUT(x, y, 0, flags, 0, 0)
    send(i)


SW, SH = user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)


def move(x, y):
    mouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, int(x * 65535 / (SW - 1)), int(y * 65535 / (SH - 1)))


SCAN = {'1': 0x02, '2': 0x03, '3': 0x04, '4': 0x05, '5': 0x06, 'q': 0x10, 'w': 0x11, 'e': 0x12, 'r': 0x13, 't': 0x14,
        'a': 0x1E, 's': 0x1F, 'd': 0x20, 'f': 0x21, 'g': 0x22, 'z': 0x2C, 'x': 0x2D, 'c': 0x2E, 'v': 0x2F, 'space': 0x39,
        'shift': 0x2A, 'tab': 0x0F, 'esc': 0x01, 'enter': 0x1C}


def key(name, up):
    i = INPUT(type=INPUT_KEYBOARD)
    i.u.ki = KEYBDINPUT(0, SCAN[name], KEYEVENTF_SCANCODE | (KEYEVENTF_KEYUP if up else 0), 0, 0)
    send(i)


def windows_of(pid):
    out = []

    @ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    def cb(h, _):
        p = w.DWORD()
        user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(h):
            out.append(h)
        return True
    user32.EnumWindows(cb, 0)
    return out


def focus(pid):
    for h in windows_of(pid):
        user32.ShowWindow(h, 9)
        user32.keybd_event(0x12, 0, 0, 0)
        user32.SetForegroundWindow(h)
        user32.keybd_event(0x12, 0, 2, 0)
        return True
    return False


def run(lines, log):
    for raw in lines:
        a = raw.split()
        if not a or a[0].startswith('#'):
            continue
        c = a[0]
        if c == 'focus':
            log.write(f'focus {a[1]} -> {focus(int(a[1]))}\n')
        elif c == 'move':
            move(float(a[1]), float(a[2]))
        elif c == 'click':
            move(float(a[1]), float(a[2]))
            time.sleep(0.05)
            right = len(a) > 3 and a[3] == 'right'
            mouse(MOUSEEVENTF_RIGHTDOWN if right else MOUSEEVENTF_LEFTDOWN)
            time.sleep(0.05)
            mouse(MOUSEEVENTF_RIGHTUP if right else MOUSEEVENTF_LEFTUP)
        elif c == 'down':
            if a[1] == 'lmb':
                mouse(MOUSEEVENTF_LEFTDOWN)
            elif a[1] == 'rmb':
                mouse(MOUSEEVENTF_RIGHTDOWN)
            else:
                key(a[1], False)
        elif c == 'up':
            if a[1] == 'lmb':
                mouse(MOUSEEVENTF_LEFTUP)
            elif a[1] == 'rmb':
                mouse(MOUSEEVENTF_RIGHTUP)
            else:
                key(a[1], True)
        elif c == 'key':
            key(a[1], False)
            time.sleep(float(a[2]) if len(a) > 2 else 0.05)
            key(a[1], True)
        elif c == 'wheel':
            i = INPUT(type=INPUT_MOUSE)
            i.u.mi = MOUSEINPUT(0, 0, ctypes.c_ulong(int(a[1]) & 0xFFFFFFFF).value, 0x0800, 0, 0)
            send(i)
        elif c == 'sleep':
            time.sleep(float(a[1]))
        elif c == 'shot':
            subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', sys.argv[0].replace('drive.py', 'shot.ps1'), '-Out', a[1]], capture_output=True)
            log.write(f'shot {a[1]}\n')
        elif c == 'mark':
            log.write(f'mark {" ".join(a[1:])} {int(time.time() * 1000)}\n')
        log.flush()


if __name__ == '__main__':
    with open(sys.argv[2], 'a') as log:
        log.write(f'start {int(time.time() * 1000)} screen {SW}x{SH}\n')
        run(open(sys.argv[1]).read().splitlines(), log)
        log.write(f'end {int(time.time() * 1000)}\n')
