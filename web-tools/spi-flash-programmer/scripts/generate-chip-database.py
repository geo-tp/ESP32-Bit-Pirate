#!/usr/bin/env python3
"""Extract a conservative SPI NOR subset from an exact flashrom checkout.
Usage: python3 scripts/generate-chip-database.py /path/to/flashrom
No network access, capacity inference or JEDEC family expansion.
"""
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REVISION = '399bffcb7d0dc3de96223163e3477f1137df319f'
BASE = Path(__file__).resolve().parents[1]

def uncomment(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)

def generate(root):
    revision = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REVISION:
        raise ValueError(f'Expected flashrom {REVISION}, got {revision}')
    if subprocess.check_output(['git', '-C', str(root), 'status', '--porcelain'], text=True).strip():
        raise ValueError('Reference checkout must be clean')
    header = uncomment((root / 'include/flashchips.h').read_text())
    constants = dict(re.findall(r'^#define\s+(\w+)\s+(0x[0-9a-fA-F]+|\d+)\b', header, re.M))
    candidates = defaultdict(list)
    excluded = []
    notices = set()
    for path in sorted((root / 'flashchips').glob('*.c')):
        original = path.read_text()
        text = uncomment(original)
        for match in re.finditer(r'^\t\{\n(.*?)^\t\},', text, re.M | re.S):
            block = match.group(1)
            def field(name):
                found = re.search(r'\.' + name + r'\s*=\s*([^,]+),', block)
                return found.group(1).strip() if found else ''
            def number(name):
                value = field(name)
                return int(constants.get(value, value), 0)
            if field('bustype') != 'BUS_SPI' or field('probe') != 'PROBE_SPI_RDID':
                continue
            try:
                manufacturer, device = number('manufacture_id'), number('model_id')
                capacity, page = number('total_size') * 1024, number('page_size')
            except ValueError:
                continue
            if not 0 < manufacturer < 256 or not 0 < device < 65535:
                continue
            key = f'{manufacturer:02x}{device:04x}'
            voltage = re.search(r'\.voltage\s*=\s*\{\s*(\d+),\s*(\d+)\s*\}', block)
            voltage = list(map(int, voltage.groups())) if voltage else None
            features = set(re.findall(r'FEATURE_\w+', field('feature_bits')))
            reasons = []
            if page != 256 or field('write') != 'SPI_CHIP_WRITE256' or field('read') != 'SPI_CHIP_READ':
                reasons.append('nonstandard read/program profile')
            if not re.search(r'\.block_erase\s*=\s*SPI_BLOCK_ERASE_C7\s*,', block):
                reasons.append('no documented C7 erase')
            if not voltage or not voltage[0] <= 3300 <= voltage[1]:
                reasons.append('not a documented 3.3 V part')
            if capacity > 16 * 1024 * 1024:
                native = bool(features & {'FEATURE_4BA', 'FEATURE_4BA_NATIVE', 'FEATURE_4BA_WREN', 'FEATURE_4BA_EAR7'}) or {'FEATURE_4BA_READ', 'FEATURE_4BA_WRITE'} <= features
                if not native:
                    reasons.append('no documented native 13/12 addressing')
            if capacity <= 0 or capacity > 64 * 1024 * 1024:
                reasons.append('outside supported allocation range (up to 64 MiB)')
            if 'FEATURE_STATUS_PER_DIE' in features or field('prepare_access') or field('finish_access'):
                reasons.append('requires special access handling')
            record = dict(manufacturer=field('vendor').strip('"'), model=field('name').strip('"'), capacity=capacity,
                          voltageMv=voltage, upstreamTested=field('tested'), source=f'flashchips/{path.name}')
            candidates[key].append((record, reasons))
        notices.update(re.findall(r'^ \* SPDX-FileCopyrightText: (.+)$', original, re.M))
    rows = []
    for key, entries in sorted(candidates.items()):
        reasons = sorted({reason for _, why in entries for reason in why})
        if len({r['capacity'] for r, _ in entries}) != 1:
            reasons.append('ambiguous capacity for the same JEDEC ID')
        if reasons:
            excluded.append(dict(jedec=key, models=[r['model'] for r, _ in entries], reasons=reasons))
            continue
        records = [r for r, _ in entries]
        rows.append([key, dict(manufacturer=' / '.join(dict.fromkeys(r['manufacturer'] for r in records)),
                               model=' / '.join(dict.fromkeys(r['model'] for r in records)),
                               capacity=records[0]['capacity'], variants=records)])
    metadata = dict(upstream='https://github.com/flashrom/flashrom', revision=REVISION,
                    license='GPL-2.0-or-later', entries=rows, excluded=excluded)
    (BASE / 'data/chip-database-provenance.json').write_text(json.dumps(metadata, indent=2) + '\n')
    lines = ['// SPDX-License-Identifier: GPL-2.0-or-later',
             '// Generated from flashrom; see ../data/README.md and chip-database-provenance.json.',
             f'// Upstream revision: {REVISION}',
             '// Regenerate with scripts/generate-chip-database.py; do not edit entries manually.',
             'const jedecChips = new Map([']
    for key, row in rows:
        runtime = {k: row[k] for k in ('manufacturer', 'model', 'capacity')}
        lines.append('  ' + json.dumps([key, runtime], ensure_ascii=False) + ',')
    lines += [']);', '', 'export function identifyJedec(jedecId) {',
              '  if (!jedecId || jedecId.length !== 3) return null;',
              '  const bytes = Array.from(jedecId);',
              '  if (!bytes.every((byte) => Number.isInteger(byte) && byte >= 0 && byte <= 255)) return null;',
              '  const key = bytes.map((byte) => byte.toString(16).padStart(2, "0")).join("");',
              '  const chip = jedecChips.get(key);',
              '  return chip ? { ...chip } : null;', '}', '']
    (BASE / 'src/chipDatabase.js').write_text('\n'.join(lines))
    (BASE / 'data/flashrom-COPYING.rst').write_text((root / 'COPYING.rst').read_text())
    (BASE / 'data/flashrom-NOTICE.txt').write_text('Derived from flashrom, GPL-2.0-or-later.\nUpstream copyright notices:\n' + '\n'.join(sorted(notices)) + '\n')
    print(f'{len(rows)} JEDEC IDs, {sum(len(r[1]["variants"]) for r in rows)} source entries; {len(excluded)} IDs excluded')

if __name__ == '__main__':
    generate(Path(sys.argv[1]))
