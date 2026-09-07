from pathlib import Path
P=Path(__file__).resolve().parents[1];p=P/'scripts/call_tower.py';s=p.read_text();s=s.replace("raw=out['result']['content'][0]['text'];d=json.loads(raw)","raw=out['result']['content'][0]['text'];d=[{'name':line[2:].split(':')[0]} for line in raw.splitlines() if line.startswith('- ')]")
p.write_text(s)
