"""Production readers/admission, no game, SDL, device enumeration, input or hooks."""
import json, pathlib, sys, tempfile, time, unittest
from shared import gate, progress, read
from owner import snapshot

class Admission(unittest.TestCase):
    def test_guard(self):
        with tempfile.TemporaryDirectory(dir=pathlib.Path.cwd()) as tmp:
            p = pathlib.Path(tmp) / 'guard.json'
            for ok, pending, age, accepted in [(True,False,0,True),(False,False,0,False),(True,True,0,False),(True,False,2.01,False),(True,False,-1,False)]:
                p.write_text(json.dumps(dict(unix=100-age,ok=ok,pending=pending)))
                if accepted: self.assertTrue(gate(p,100)['ok'])
                else:
                    with self.assertRaises(RuntimeError): gate(p,100)
            p.write_text(json.dumps(dict(unix=100,ok=True,pending=True)))
            self.assertTrue(gate(p,100,require_cleared=False)['pending'])
    def test_readback(self):
        row=dict(session='a'*32,sequence=1,unixMs=100000,phase='running',frames=600,presents=610,
            neutralInput=True,forceMuted=True,deviceInput=False,network=False,hardware=True)
        def text(r): return json.dumps(r)+'\n'
        self.assertEqual(progress(text(row)+'{"partial":',row['session'],clock=100)['frames'],600)
        self.assertEqual(progress(text(dict(row,phase='starting',frames=0)),row['session'],clock=180)['frames'],0)
        with self.assertRaises(RuntimeError): progress(text(dict(row,phase='starting',frames=0)),row['session'],clock=191)
        for key, value in [('session','b'*32),('unixMs',96000),('neutralInput',False),('forceMuted',False),('deviceInput',True),('network',True)]:
            with self.assertRaises(RuntimeError): progress(text(dict(row,**{key:value})),row['session'],clock=100)
        with self.assertRaises(RuntimeError): progress(text(row),row['session'],previous=2,clock=100)
        with self.assertRaises(RuntimeError): progress(text(row)+text(dict(row,sequence=2,frames=599)),row['session'],clock=100)
    def test_owner(self):
        with tempfile.TemporaryDirectory(dir=pathlib.Path.cwd()) as tmp:
            p=pathlib.Path(tmp); roots=[tmp,str(p/'absent')]
            initial=snapshot(roots); self.assertEqual(snapshot(roots),initial)
            (p/'new-empty').mkdir(); self.assertNotEqual(snapshot(roots),initial)
            (p/'file').write_bytes(b'payload'); one=snapshot(roots)
            (p/'file').write_bytes(b'changed'); self.assertNotEqual(snapshot(roots),one)

if __name__=='__main__':
    if len(sys.argv)>1 and sys.argv[1]=='--watch':
        p,out=map(pathlib.Path,sys.argv[2:]); start=time.monotonic(); reads=0; errors=[]
        while time.monotonic()-start<3:
            try: json.loads(read(p)); reads+=1
            except BaseException as e: errors.append(str(e))
        out.write_text(json.dumps(dict(reads=reads,errors=errors)))
        sys.exit(bool(errors) or reads==0)
    unittest.main()
