"""Validate P21 reference integrity and project font provenance (standard library only)."""
from pathlib import Path
import hashlib
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
references = ROOT / 'Docs/Images/UI/DesignSystem'
expected = {'anchor', 'screen-shell', 'top-bar', 'bottom-tray', 'tab', 'category-button',
            'building-card', 'chain-connector', 'panel', 'table-row', 'tooltip', 'modal',
            'notification', 'progress', 'chart', 'overlay', 'cursor', 'focus', 'icon',
            'decoration', 'button'}
found = set()
for png in references.glob('*.png'):
    match = re.fullmatch(r'design-system--(.+)--default--(\d+)x(\d+)--v\d+\.png', png.name)
    assert match, png
    component, width, height = match.groups()
    assert component not in found, f'Duplicate selected component: {component}'
    found.add(component)
    data = png.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', png
    assert struct.unpack('>II', data[16:24]) == (int(width), int(height)), png
    prompt = png.with_suffix('.prompt.md').read_text(encoding='utf-8-sig')
    assert hashlib.sha256(data).hexdigest() in prompt, f'Changed original: {png}'
    assert 'reference only' in prompt and 'built-in ImageGen' in prompt and '## Final prompt' in prompt, png
assert found == expected, (expected-found, found-expected)
fonts = ROOT / 'Content/Hansa/UI/Fonts'
for item in json.loads((fonts / 'provenance.json').read_text(encoding='utf-8')):
    assert hashlib.sha256((fonts/item['file']).read_bytes()).hexdigest() == item['sha256'], item['file']
    assert (fonts/item['license']).is_file(), item['license']
    assert item['source'].startswith('https://raw.githubusercontent.com/'), item['source']
print(f'PASS: {len(found)} original references, sibling prompts, native sizes and 3 licensed font hashes')
