#!/usr/bin/env python3
"""Turn build/sim/state_*.raw + states.json into a clickable web preview.

One self-contained index.html: the current screen is shown at device aspect
with transparent click targets over every HitRegion, so tapping in the browser
walks the same state machine the firmware runs.
"""
import base64
import io
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SIM = ROOT / "build" / "sim"
SCALE = 0.62

HTML = """<!doctype html><html><head><meta charset="utf-8">
<title>me.status preview</title>
<style>
 body{font:14px/1.5 -apple-system,sans-serif;margin:0;background:#f4f4f6;color:#111}
 #wrap{display:flex;gap:24px;padding:24px}
 #side{width:230px;flex:none;position:sticky;top:24px;max-height:calc(100vh - 48px);overflow:auto}
 #side h1{font-size:16px;margin:0 0 8px}
 #side ol{padding-left:20px;margin:0}
 #side li{margin:2px 0;cursor:pointer}
 #side li.cur{font-weight:700}
 #stage{position:relative;flex:none;background:#fff;box-shadow:0 2px 12px rgba(0,0,0,.15)}
 #stage img{display:block}
 .hot{position:absolute;cursor:pointer;outline:1px dashed transparent}
 .hot:hover{outline:2px dashed #e0447a;background:rgba(224,68,122,.12)}
 #meta{margin-top:12px;color:#555}
</style></head><body><div id="wrap">
<div id="side"><h1>me.status — web preview</h1>
<p>点屏幕上的虚线热区即可走流程；渲染来自与固件同一份 C++ 页面代码。</p>
<ol id="list"></ol><div id="meta"></div></div>
<div><div id="stage"></div></div>
</div><script>
const SCALE=__SCALE__;
const STATES=__STATES__;
let cur=0;
const stage=document.getElementById('stage');
const list=document.getElementById('list');
function show(i){
  cur=i; const s=STATES[i];
  stage.innerHTML='';
  const img=new Image();
  img.src=s.png;
  img.style.width=(480*SCALE)+'px';
  stage.appendChild(img);
  for(const r of s.regions){
    const t=s.taps.find(t=>t.id===r.id);
    if(!t) continue;
    const d=document.createElement('div');
    d.className='hot'; d.title=r.id+' -> state '+t.to;
    d.style.left=(r.x*SCALE)+'px'; d.style.top=(r.y*SCALE)+'px';
    d.style.width=(r.w*SCALE)+'px'; d.style.height=(r.h*SCALE)+'px';
    d.onclick=()=>show(t.to);
    stage.appendChild(d);
  }
  document.getElementById('meta').textContent =
    'state '+i+'  page='+s.page+'  mood='+s.mood+'  energy='+s.energy+'  intention='+s.intention;
  [...list.children].forEach((li,j)=>li.className=j===i?'cur':'');
}
STATES.forEach((s,i)=>{
  const li=document.createElement('li');
  li.textContent=i+': '+s.page+' '+s.mood+'/'+s.energy+'/'+s.intention;
  li.onclick=()=>show(i);
  list.appendChild(li);
});
show(0);
</script></body></html>
"""


def main():
    text = (SIM / "states.json").read_text().replace(",\n]", "\n]")
    states = json.loads(text)
    entries = []
    for s in states:
        raw = (SIM / f"state_{s['i']:03d}.raw").read_bytes()
        mask = np.unpackbits(np.frombuffer(raw, dtype=np.uint8), bitorder="big")
        mask = mask.reshape(800, 480)[:800, :480]
        img = Image.fromarray(np.where(mask, 0, 255).astype(np.uint8), "L")
        buf = io.BytesIO()
        img.save(buf, format="PNG", optimize=True)
        s2 = dict(s)
        s2["png"] = "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()
        entries.append(s2)
    html = HTML.replace("__SCALE__", str(SCALE)).replace("__STATES__", json.dumps(entries))
    (SIM / "index.html").write_bytes(html.encode())
    print(f"{len(entries)} states -> {SIM / 'index.html'}")


if __name__ == "__main__":
    sys.exit(main())
