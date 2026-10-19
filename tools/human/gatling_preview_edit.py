import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gx_preview_edit as ed

ed.WORK = r'D:\MxM\showcase_data\preview_gatling'
ed.PREFIX = 'gatp'
ed.ACCENT = '0xe6553a'

SEGMENTS = [
    ('take0', 'lmb', -4.4, 5.0, 'WEAPON', 'Both barrels fire in turn - each hit lowers defense by 1.5%'),
    ('take0', 'rmb', -0.8, 3.0, 'SPIDER MINE', 'Dashes 6 m forward and marks the ground with a spider mine'),
    ('take0', 'q', -1.0, 4.5, 'SKILL 1 - ANCHOR THROW', 'Steps back and hurls an anchor 15.75 m forward'),
    ('take0', 'e', -1.0, 4.0, 'SKILL 2 - BACK GRAB', 'Dashes through the target, then a burst that knocks enemies down'),
    ('take1', 'q', -1.0, 4.5, 'SKILL 3 - LEAP BARRAGE', 'Leaps and sprays a half circle 4 times - 4 hits slow by 50%'),
    ('take1', 'e', -1.0, 6.0, 'SKILL 4 - HEAVY SNIPE', 'Marks the target for 3 s - then an explosion within 6 m'),
    ('take0', 'r', -1.0, 6.5, 'ULTIMATE - HEAVY BARRAGE', 'Super Armor - fires all around every 0.4 s, pushing enemies out'),
]


def main():
    parts = []
    a0 = ed.acts('take0')
    intro_at = (a0['lmb'] - ed.video_start('take0')) / 1000.0 - 9.0
    parts.append(ed.card(0, 'take0', intro_at, 3.5, [
        ('YEON HWARIN', 140, ed.BOLD, '(h/2)-240', 'white'),
        ('GATLING', 44, ed.BOLD, '(h/2)-72', ed.ACCENT),
        ('Assault  -  Mid-ranged', 34, ed.TEXT, '(h/2)+40', '0xd8e8f4'),
    ]))
    for i, s in enumerate(SEGMENTS):
        parts.append(ed.segment(i, *s))
    outro_at = (a0['r'] - ed.video_start('take0')) / 1000.0 + 6.0
    parts.append(ed.card(1, 'take0', outro_at, 3.0, [
        ('YEON HWARIN', 120, ed.BOLD, '(h/2)-160', 'white'),
        ('Ported from the CN client to MXM EU', 36, ed.TEXT, '(h/2)+30', '0xd8e8f4'),
    ]))
    lst = os.path.join(ed.WORK, 'parts.txt')
    with open(lst, 'w', encoding='utf-8') as f:
        for p in parts:
            f.write("file '%s'\n" % p.replace('\\', '/'))
    out = os.path.join(ed.WORK, 'Gatling_preview.mp4')
    ed.run(['-f', 'concat', '-safe', '0', '-i', lst, '-c', 'copy', out])
    print(out)


if __name__ == '__main__':
    main()
