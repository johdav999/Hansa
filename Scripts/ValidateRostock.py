"""Validate and preserve original P31 viewport evidence; never grants art approval."""
import hashlib, json, shutil, struct
from pathlib import Path
from PIL import Image
STATES = ("home", "visited", "waterfront", "market", "approaching", "berthed", "delivered", "restored", "returned")
SIZES = ((1280,720),(1920,1080),(2560,1440),(3440,1440))
def metrics(path):
    raw=path.read_bytes()
    return dict(line.split("=",1) for line in raw.decode("utf-16" if raw.startswith((b"\xff\xfe",b"\xfe\xff")) else "utf-8-sig").splitlines() if "=" in line)
def main():
    source=Path("Saved/P31");out=Path("Docs/Images/World/RostockP31");rows=[]
    for w,h in SIZES:
        group={}
        for state in STATES:
            stem=f"rostock-{w}x{h}-{state}";raw=(source/(stem+".png")).read_bytes()
            assert raw[:8]==b"\x89PNG\r\n\x1a\n" and struct.unpack(">II",raw[16:24])==(w,h),stem
            assert Image.open(source/(stem+".png")).getchannel("A").getextrema()==(255,255),(stem,"non-opaque viewport")
            m=metrics(source/(stem+".txt"));group[state]=m
            assert m["city"]==("City.Lubeck" if state in ("home","returned") else "City.Rostock"),stem
            if state not in ("home","returned"):
                assert int(m["engineShapes"])==0 and int(m["lod0UpperBoundTriangles"])<1_000_000,stem
                assert 0<float(m["gameThreadMeanMs"])<1000 and 0<float(m["renderThreadMeanMs"])<1000,stem
            rows.append(dict(capture=stem,sha256=hashlib.sha256(raw).hexdigest(),**m))
        assert len({group[s]["fingerprint"] for s in STATES[:4]})==1
        assert len({group[s]["fingerprint"] for s in STATES[6:]})==1
        assert int(group["delivered"]["transferQuantity"])>0 and group["delivered"]["transferCity"]=="City.Rostock"
        assert "1 at berth" in group["berthed"]["arrival"] and "10 units aboard" in group["berthed"]["arrival"]
    for state in STATES:
        assert len({r["fingerprint"] for r in rows if r["state"]==state})==1,(state,"resolution affected authority")
    out.mkdir(parents=True,exist_ok=True)
    for row in rows:
        for suffix in (".png",".txt",".meshes.tsv"):
            f=source/(row["capture"]+suffix);shutil.copyfile(f,out/f.name)
    if (out/"Scale14").exists():
        for state in STATES:
            stem=f"rostock-1280x720-{state}";png=out/"Scale14"/(stem+".png");raw=png.read_bytes();im=Image.open(png)
            assert im.size==(1280,720) and im.getchannel("A").getextrema()==(255,255),png
            m=metrics(png.with_suffix(".txt"));base=next(r for r in rows if r["capture"]==stem)
            assert m["fingerprint"]==base["fingerprint"],(state,"UI scale affected authority")
            rows.append(dict(capture="Scale14/"+stem,uiScale=1.4,sha256=hashlib.sha256(raw).hexdigest(),**m))
    (out/"validation.json").write_text(json.dumps(dict(technicalEvidencePassed=True,releaseAccepted=False,captureCount=len(rows),captures=rows,limitations=["Staged interpreted quarter, not surveyed medieval reconstruction", "Warehouse building and vegetation family absent", "CPU samples and LOD0 inventory are not GPU or draw-call acceptance", "Production promotion requires reviewed approval"]),indent=2)+"\n")
    print(f"Validated {len(rows)} original-size captures; matching authoritative states across resolutions. Release acceptance remains open.")
if __name__=="__main__":main()
