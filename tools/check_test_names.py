#!/usr/bin/env python3
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'src'

pattern = re.compile(r"TEST_CASE\(\s*(?:\"(?P<quoted>[^\"]+)\"|(?P<bare>[A-Za-z0-9_]+))\s*\)")

bad = []
for p in SRC.rglob('*.c'):
    text = p.read_text(encoding='utf-8', errors='ignore')
    for m in pattern.finditer(text):
        name = m.group('quoted') if m.group('quoted') else m.group('bare')
        # Valid if contains only [A-Za-z0-9_] and has at least one underscore
        if not re.match(r'^[A-Za-z0-9_]+$', name) or '_' not in name:
            bad.append((p.relative_to(ROOT), name))

if bad:
    print('Found tests that do not follow <module>_<feature> naming:')
    for f, n in bad:
        print(f'  {f}: {n}')
    sys.exit(2)
else:
    print('All test names follow <module>_<feature> convention')
    sys.exit(0)
