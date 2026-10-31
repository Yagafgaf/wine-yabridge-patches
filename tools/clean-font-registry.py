# Dry run / apply: drop external (Z:\) font entries from the prefix registry files.
import os, re, sys, collections
APPLY = len(sys.argv) > 1 and sys.argv[1] == 'apply'
P = os.environ['WINEPREFIX'].rstrip('/') + '/'  # run only with wineserver stopped; back up *.reg first
KEYS = {
  'system.reg': [r'[Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts]',
                 r'[Software\\Microsoft\\Windows\\CurrentVersion\\Fonts]'],
  'user.reg':   [r'[Software\\Wine\\Fonts\\External Fonts]'],
}
for fn, keys in KEYS.items():
    lines = open(P + fn, encoding='utf-8', errors='surrogateescape').read().split('\n')
    out, cur, removed = [], None, collections.Counter()
    for l in lines:
        if l.startswith('['):
            cur = next((k for k in keys if l.startswith(k)), None)
        m = re.match(r'^".*"="(.*)"$', l)
        if cur and m and m.group(1).upper().startswith('Z:'):
            d = re.sub(r'\\\\[^\\]*$', '', m.group(1))
            removed[d] += 1
            continue
        out.append(l)
    print(fn, sum(removed.values()), 'entries')
    for d, n in removed.most_common(12): print('   %4d %s' % (n, d))
    if APPLY: open(P + fn, 'w', encoding='utf-8', errors='surrogateescape').write('\n'.join(out))
