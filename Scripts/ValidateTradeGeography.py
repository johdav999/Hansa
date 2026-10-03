"""Offline TG-03 source/coverage check. No provider calls or content promotion."""
from pathlib import Path
import hashlib
import json
import math
import re
import zipfile

root = Path(__file__).resolve().parents[1]
source = root / 'SourceArt/UI/TradeWorkspace/Geography'
review = json.loads((source / 'reviewed-city-locations.json').read_text(encoding='utf-8'))
archive = source / 'cities15000.zip'
assert hashlib.sha256(archive.read_bytes()).hexdigest() == review['source_sha256']
with zipfile.ZipFile(archive) as data:
    rows = {row[0]: row for row in (line.split('\t') for line in data.read('cities15000.txt').decode().splitlines())}
cities = {city['id']: city for city in review['cities']}
assert len(cities) == len(review['cities']) == 39
for city in cities.values():
    gazetteer = rows[str(city['geonames_id'])]
    assert city['geonames_name'] == gazetteer[1]
    assert city['reference_latitude'] == float(gazetteer[4])
    assert city['reference_longitude'] == float(gazetteer[5])
    offset = math.hypot((city['latitude'] - float(gazetteer[4])) * 111.2,
                        (city['longitude'] - float(gazetteer[5])) * 111.2 * math.cos(math.radians(city['latitude'])))
    assert offset < 6, (city['id'], offset)
    assert -1 <= city['longitude'] <= 32 and 50 <= city['latitude'] <= 61

authored = (root / 'Source/HansaEditor/Private/Definitions/HansaRegionalProductionDraft.cpp').read_text(encoding='utf-8')
scope = authored[authored.index('const TArray<FCity> Cities='):authored.index('auto* Template=')]
current = {'City.' + name for name in re.findall(r'\{TEXT\("([^"]+)"\),TEXT\("[^"]+ market"\)', scope)}
assert len(current) == 30 and current <= cities.keys(), current - cities.keys()
generated = (root / 'Source/Hansa/Private/UI/HansaTradeCityLocations.inl').read_text(encoding='utf-8')
for city in cities.values():
    expected = '{TEXT("%s"),FVector2D(%.7f,%.7f)},' % (city['id'], (city['longitude'] + 1) / 33, (61 - city['latitude']) / 11)
    assert expected in generated, city['id']
print(json.dumps({'status': 'passed', 'reviewed_locations': len(cities), 'current_catalog_cities': len(current),
                  'source_sha256': review['source_sha256'], 'checks': ['named gazetteer identity', 'approximate centre tolerance <6km',
                  'chart extent', 'unique stable IDs', 'current catalog coverage', 'generated lookup matches source']}, indent=2))
