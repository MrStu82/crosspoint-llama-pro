from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[2]; here=Path(__file__).resolve().parent
flags=sys.argv[1:]
with tempfile.TemporaryDirectory() as d:
 exe=str(Path(d)/'FinishedBookTest')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror']+flags+['-I'+str(here/'stubs'),'-I'+str(r/'src'),str(here/'FinishedBookTest.cpp')]+[str(r/'src/util'/f) for f in ('BookReadingStats.cpp','ChapterProgress.cpp','BookProgressBadge.cpp')]+['-o',exe],check=True)
 subprocess.run([exe],check=True)
