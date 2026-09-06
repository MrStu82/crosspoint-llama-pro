from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]; here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as d:
 for name in ('RecoveryTest','FingerprintTest','StatsTest'):
  exe=str(Path(d)/name)
  subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(here/'stubs'),'-I'+str(r),str(here/(name+'.cpp'))]+([str(r/'src/util/BookReadingStats.cpp'),'-I'+str(r/'src')] if name=='StatsTest' else [])+['-o',exe],check=True)
  subprocess.run([exe],check=True)
