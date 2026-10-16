import collections, os, re, sys

logs = sys.argv[1:]
known_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'known_categories.txt')
known = set(l.strip() for l in open(known_path, encoding='utf-8')) if os.path.exists(known_path) else set()

alert = re.compile(r'WARN|ERROR|FAILED|unknown|unhandled|not handled|missing|invalid|assert|TODO|FIXME', re.I)
numbers = re.compile(r'-?\d+(\.\d+)?')
categories = collections.Counter()
alerts = collections.Counter()
samples = {}
for path in logs:
    for line in open(path, encoding='utf-8', errors='replace'):
        body = re.sub(r'^\[[0-9a-f]+\]\s*', '', line.rstrip())
        m = re.match(r'([A-Za-z][A-Za-z ]{1,30}?) ::', body)
        if m:
            categories[m.group(1)] += 1
        if alert.search(body):
            key = numbers.sub('N', body)[:160]
            alerts[key] += 1
            samples.setdefault(key, body[:220])

print('== categories')
for c, n in categories.most_common():
    print(f'{n:7d}  {c}{"" if c in known else "   <-- NEW"}')
print('== alerts')
for k, n in alerts.most_common(40):
    print(f'{n:7d}  {samples[k]}')
if '--learn' in os.environ.get('LOGSCAN', ''):
    with open(known_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(sorted(known | set(categories))) + '\n')
