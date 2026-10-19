import os
import re
import subprocess
import sys

FFMPEG = r'D:\MxM\showcase_data\venv\Lib\site-packages\imageio_ffmpeg\binaries\ffmpeg-win-x86_64-v7.1.exe'
WORK = r'D:\MxM\showcase_data\preview_gx09'
RUNS = r'D:\MxM\showcase_data\runs'
BOLD = 'C\\:/Windows/Fonts/bahnschrift.ttf'
TEXT = 'C\\:/Windows/Fonts/segoeui.ttf'
ACCENT = '0x35a6e6'
FADE = 0.35
FPS = 60

SEGMENTS = [
    ('take0', 'lmb', -2.8, 4.0, 'WEAPON', 'Cone attack in front of GX-09'),
    ('take0', 'lmbhold', -2.4, 4.0, 'CHARGED ATTACK', 'Recovers 20 Fuel - 5 Fuel per second while charging'),
    ('take0', 'rmb', -0.8, 2.8, 'SUDDEN ACCELERATION', 'Dash 8 m with Super Armor - 22 Fuel per enemy in the path'),
    ('take0', 'q', -1.0, 5.0, 'SKILL 1 - DISRUPTION ROUTE', 'Distorted trail: enemies slowed and pulled to its center'),
    ('take0', 'e', -1.0, 4.5, 'SKILL 2 - B-EMP', 'Drains 30% of the MP and EP of enemy masters within 4 m'),
    ('take1', 'q', -0.8, 3.5, 'SKILL 3 - SHOCKWAVE', 'Knocks enemies back to the edge of the wave'),
    ('take1', 'e', -0.8, 5.5, 'SKILL 4 - MASTER TRANSPORT', 'Allies board with the interaction key for 3 s - half speed while transforming'),
    ('take1', 'drive', -3.0, 5.0, 'MASTER TRANSPORT', '+40% move speed after 2 s - the passenger is dropped when the 10 s end'),
    ('take1', 'r', -1.0, 6.0, 'ULTIMATE - TELEPORTER', 'Recalls the allies near the teleporter to GX-09 after 3 s'),
]


def run(args):
    r = subprocess.run([FFMPEG, '-v', 'error', '-y'] + args, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(r.stderr)


def acts(tag):
    out = {}
    for line in open(os.path.join(RUNS, 'gxp%s_agent.log' % tag[-1]), encoding='utf-8'):
        m = re.match(r'acted key=(\S+) at=\S+ t=(\d+)', line)
        if m:
            out.setdefault(m.group(1), int(m.group(2)))
    return out


def video_start(tag):
    return int(open(os.path.join(WORK, tag + '.mkv.start'), encoding='utf-8-sig').read().strip())


def esc(s):
    return s.replace('\\', '\\\\').replace(':', '\\:').replace("'", "\u2019").replace('%', '\\%')


def caption(title, sub, dur):
    alpha = "if(lt(t,0.25),0,if(lt(t,0.6),(t-0.25)/0.35,if(gt(t,%.2f),max(0,(%.2f-t)/0.35),1)))" % (dur - 0.6, dur - 0.25)
    return ("drawbox=x=0:y=ih-330:w=880:h=120:color=black@0.55:t=fill,"
            "drawbox=x=60:y=ih-318:w=8:h=96:color=%s@1:t=fill,"
            "drawtext=fontfile='%s':text='%s':fontsize=52:fontcolor=white:x=88:y=h-320:alpha='%s',"
            "drawtext=fontfile='%s':text='%s':fontsize=26:fontcolor=0xd8e8f4:x=90:y=h-258:alpha='%s'"
            % (ACCENT, BOLD, esc(title), alpha, TEXT, esc(sub), alpha))


def segment(i, tag, key, off, dur, title, sub):
    start = (acts(tag)[key] - video_start(tag)) / 1000.0 + off
    out = os.path.join(WORK, 'seg%02d.mp4' % i)
    vf = 'fps=%d,scale=1920:1080,delogo=x=1512:y=944:w=340:h=66,%s,fade=t=in:st=0:d=%.2f,fade=t=out:st=%.2f:d=%.2f' % (FPS, caption(title, sub, dur), FADE, dur - FADE, FADE)
    run(['-ss', '%.3f' % start, '-i', os.path.join(WORK, tag + '.mkv'), '-t', '%.3f' % dur, '-vf', vf, '-an',
         '-c:v', 'libx264', '-preset', 'slow', '-crf', '18', '-pix_fmt', 'yuv420p', out])
    return out


def card(i, tag, at, dur, lines):
    out = os.path.join(WORK, 'card%02d.mp4' % i)
    draws = []
    for text, size, font, y, color in lines:
        draws.append("drawtext=fontfile='%s':text='%s':fontsize=%d:fontcolor=%s:x=(w-text_w)/2:y=%s" % (font, esc(text), size, color, y))
    vf = 'fps=%d,scale=1920:1080,boxblur=12:2,eq=brightness=-0.25:saturation=0.8,drawbox=x=(iw-520)/2:y=ih/2+8:w=520:h=6:color=%s@1:t=fill,%s,fade=t=in:st=0:d=0.5,fade=t=out:st=%.2f:d=0.5' % (FPS, ACCENT, ','.join(draws), dur - 0.5)
    run(['-ss', '%.3f' % at, '-i', os.path.join(WORK, tag + '.mkv'), '-t', '%.3f' % dur, '-vf', vf, '-an',
         '-c:v', 'libx264', '-preset', 'slow', '-crf', '18', '-pix_fmt', 'yuv420p', out])
    return out


def main():
    parts = []
    a0 = acts('take0')
    intro_at = (a0['lmb'] - video_start('take0')) / 1000.0 - 6.0
    parts.append(card(0, 'take0', intro_at, 3.5, [
        ('GX-09', 150, BOLD, '(h/2)-190', 'white'),
        ('MASTER TRANSPORT ROBOT', 44, BOLD, '(h/2)-30', ACCENT),
        ('Specialist  -  Melee  -  Kinetic', 34, TEXT, '(h/2)+40', '0xd8e8f4'),
    ]))
    for i, s in enumerate(SEGMENTS):
        parts.append(segment(i, *s))
    a1 = acts('take1')
    outro_at = (a1['r'] - video_start('take1')) / 1000.0 + 5.5
    parts.append(card(1, 'take1', outro_at, 3.0, [
        ('GX-09', 120, BOLD, '(h/2)-160', 'white'),
        ('Ported from the CN client to MXM EU', 36, TEXT, '(h/2)+30', '0xd8e8f4'),
    ]))
    lst = os.path.join(WORK, 'parts.txt')
    with open(lst, 'w', encoding='utf-8') as f:
        for p in parts:
            f.write("file '%s'\n" % p.replace('\\', '/'))
    run(['-f', 'concat', '-safe', '0', '-i', lst, '-c', 'copy', os.path.join(WORK, 'GX09_preview.mp4')])
    print(os.path.join(WORK, 'GX09_preview.mp4'))


if __name__ == '__main__':
    main()
