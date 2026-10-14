"""Map rgrun's prof.txt (instruction samples per bank:pc) to functions via rom.noi.
Usage: python3 prof.py <artifact_dir> <rom.noi> [top_n]"""
import sys, bisect, re
d, noi = sys.argv[1], sys.argv[2]; top = int(sys.argv[3]) if len(sys.argv) > 3 else 30
syms = []
for line in open(noi):
    m = re.match(r'DEF (\S+) 0x([0-9A-Fa-f]+)', line)
    if not m: continue
    n, a = m.group(1), int(m.group(2), 16)
    if n.startswith('F') and '$' in n: n = n.split('$')[1]          # static function
    elif n.startswith('_') and not n.startswith('__'): n = n[1:]     # global
    else: continue
    if (a & 0xFFFF) >= 0x8000: continue                              # RAM symbols
    syms.append((a, n))
syms.sort(); addrs = [a for a, _ in syms]
tot = {}; total = 0
for line in open(d + '/prof.txt'):
    a, c = line.split(); a = int(a, 16); c = int(c); total += c
    i = bisect.bisect_right(addrs, a) - 1
    name = syms[i][1] if i >= 0 and (syms[i][0] >> 16) == (a >> 16) else '?%x' % a
    tot[name] = tot.get(name, 0) + c
for n, c in sorted(tot.items(), key=lambda x: -x[1])[:top]:
    print('%6.2f%%  %s' % (100.0 * c / total, n))
