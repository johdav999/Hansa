"""Fetch OSM geometry for the three named rivers absent from Natural Earth."""
import json
import hashlib
import urllib.request
import urllib.parse
from pathlib import Path
from datetime import datetime, timezone

ROOT=Path(__file__).resolve().parents[2]/'SourceArt/Terrain/HansaWorld/Prototype_20260918/sources'
QUERIES={
    'trave-warnow':'[out:json][timeout:90];way[waterway=river][name~"^(Trave|Warnow)$"](52,8,55,14);out geom;',
    'pregel':'[out:json][timeout:90];way[waterway=river][name~"Преголя|Pregel|Pregolya"](54,19,56,23);out geom;'
}
URL='https://overpass-api.de/api/interpreter'
features=[]
for key,query in QUERIES.items():
    path=ROOT/(key+'.osm.json')
    if not path.exists():
        request=urllib.request.Request(URL+'?'+urllib.parse.urlencode({'data':query}),headers={'User-Agent':'HansaTerrain/1.0'})
        with urllib.request.urlopen(request,timeout=120) as response:
            path.write_bytes(response.read())
    source=json.loads(path.read_text(encoding='utf-8'))
    for element in source['elements']:
        coords=[[p['lon'],p['lat']] for p in element.get('geometry',[])]
        if len(coords)>1:
            features.append({'type':'Feature','geometry':{'type':'LineString','coordinates':coords},
                             'properties':{'name':element['tags'].get('name'),'osm_way_id':element['id'],
                                           'source':'OpenStreetMap contributors, ODbL 1.0; '+URL}})
    metadata={'url':URL,'query':query,'accessed_utc':datetime.now(timezone.utc).isoformat(),
              'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'license':'ODbL 1.0',
              'attribution':'© OpenStreetMap contributors','license_url':'https://www.openstreetmap.org/copyright'}
    path.with_suffix('.source.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
(ROOT/'supplemental-rivers.geojson').write_text(json.dumps({'type':'FeatureCollection','features':features},ensure_ascii=False),encoding='utf-8')
print('Supplemental river segments:',len(features),flush=True)
