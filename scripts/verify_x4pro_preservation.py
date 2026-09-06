#!/usr/bin/env python3
"""One bounded X4 Pro gate: exact source/SDK, hashed fixtures, existing suites."""
import argparse, hashlib, json, os, pathlib, subprocess, sys
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--expected-head',required=True);p.add_argument('--ctest-dir',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
manifest=json.loads((root/'test/x4_improvements/preservation.json').read_text())
def git(*args):return subprocess.check_output(['git',*args],cwd=root,text=True).strip()
def require(ok,why):
 if not ok:raise RuntimeError(why)
results=[]
try:
 require(git('rev-parse','HEAD')==a.expected_head,'source HEAD mismatch')
 require(git('rev-parse','HEAD:freeink-sdk')==manifest['sdk'],'SDK gitlink mismatch')
 require(git('-C','freeink-sdk','rev-parse','HEAD')==manifest['sdk'],'SDK checkout mismatch')
 # Hashes before dirty check so negative fixture proof names the real failure.
 for path,expected in manifest['sha256'].items():
  require(hashlib.sha256((root/path).read_bytes()).hexdigest()==expected,'fixture/source hash mismatch: '+path)
 require(not git('status','--porcelain'),'source dirty')
 require(not git('-C','freeink-sdk','status','--porcelain'),'SDK dirty')
 env=os.environ.copy();env['ARDUINOJSON_INCLUDE']=str(a.ctest_dir/'_deps/arduinojson-src/src')
 for i,script in enumerate(manifest['scripts']):
  command=[sys.executable,str(root/script),str(root)]
  with open(a.output/f'contract-{i}.log','w') as f:rc=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT).returncode
  results.append({'script':script,'rc':rc});require(rc==0,'contract failed: '+script)
 inventory=json.loads(subprocess.check_output(['ctest','--test-dir',str(a.ctest_dir),'--show-only=json-v1'],text=True))
 require(len(inventory['tests'])>=298,'required host suite missing tests')
 with open(a.output/'ctest.log','w') as f:rc=subprocess.run(['ctest','--test-dir',str(a.ctest_dir),'--output-on-failure'],stdout=f,stderr=subprocess.STDOUT).returncode
 require(rc==0,'CTest failed')
 result={'status':'PASS','head':a.expected_head,'sdk':manifest['sdk'],'fixture_hashes':len(manifest['sha256']),'contracts':results,'ctest_cases':len(inventory['tests'])}
except Exception as e:
 result={'status':'FAIL','error':str(e),'contracts':results}
(a.output/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result));sys.exit(0 if result['status']=='PASS' else 1)
