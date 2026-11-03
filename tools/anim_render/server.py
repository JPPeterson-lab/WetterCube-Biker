import http.server, os
ROOT=os.path.dirname(os.path.abspath(__file__))
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(s,*a,**k): super().__init__(*a,directory=ROOT,**k)
    def do_POST(s):
        rel=s.path.split('?')[0].replace('/save/','',1)
        p=os.path.join(ROOT,'frames',rel)
        os.makedirs(os.path.dirname(p),exist_ok=True)
        n=int(s.headers.get('Content-Length',0)); data=s.rfile.read(n)
        open(p,'wb').write(data); s.send_response(200); s.end_headers(); s.wfile.write(b'ok')
    def log_message(s,*a): pass
http.server.ThreadingHTTPServer(('127.0.0.1',8768),H).serve_forever()
