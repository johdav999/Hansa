import urllib.request,json,sys,pathlib
P=pathlib.Path(__file__).resolve().parents[1]
URL='http://127.0.0.1:8000/mcp'
def rpc(method,params=None,sid=None,ident=1):
 headers={'Content-Type':'application/json','Accept':'application/json, text/event-stream'}
 if sid:headers['Mcp-Session-Id']=sid
 body={'jsonrpc':'2.0','method':method}
 if params is not None:body['params']=params
 if ident is not None:body['id']=ident
 req=urllib.request.Request(URL,json.dumps(body).encode(),headers)
 with urllib.request.urlopen(req,timeout=120) as r:
  txt=r.read().decode(); session=r.headers.get('Mcp-Session-Id')
  if txt.startswith('event:') or txt.startswith('data:'):
   txt='\n'.join(l[6:] for l in txt.splitlines() if l.startswith('data: '))
  return (json.loads(txt) if txt else {}),session
if __name__=='__main__':
 result,sid=rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'HansaBakeryLocal','version':'1'}})
 rpc('notifications/initialized',sid=sid,ident=None)
 method=sys.argv[1] if len(sys.argv)>1 else 'tools/list'
 params=json.loads(sys.argv[2]) if len(sys.argv)>2 else {}
 out,_=rpc(method,params,sid,2)
 print(json.dumps(out,ensure_ascii=False))
