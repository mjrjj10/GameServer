import collections, re, sys

path = sys.argv[1]
TAG = re.compile(r'^\[[0-9a-f]+\] \[g(\d+)\] ')
out = collections.defaultdict(list)
for line in open(path, encoding='utf-8', errors='replace'):
    m = TAG.match(line)
    if m:
        out[m.group(1)].append(line)
for i, (tag, lines) in enumerate(sorted(out.items(), key=lambda kv: int(kv[0]))):
    human = ''
    for l in lines:
        m = re.search(r'Showcase :: start .* human=(\S+)', l)
        if m:
            human = m.group(1)
            break
    idx = 0 if human == 'alexis' else int(human[6:]) - 1 if human.startswith('alexis') and human[6:].isdigit() else i
    dst = path[:-4] + f'_g{idx}.log'
    with open(dst, 'w', encoding='utf-8') as f:
        f.writelines(lines)
    print(f'game {tag} ({human}): {len(lines)} lines -> {dst}')
