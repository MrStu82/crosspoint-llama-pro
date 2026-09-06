from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2];d=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as t:
 exe=str(Path(t)/'safety')
 subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(d/'stubs'),'-I'+str(r),str(d/'SafetyTest.cpp'),str(r/'lib/Xtc/Xtc/XtcParser.cpp'),'-o',exe],check=True)
 subprocess.run([exe],check=True)

with tempfile.TemporaryDirectory() as t:
 for name in ('OtaPolicyTest','JsonRecoveryTest','DiagnosticsTest'):
  exe=str(Path(t)/name)
  args=['g++','-std=c++17','-O1','-I'+str(r),str(d/(name+'.cpp'))]
  if name=='OtaPolicyTest':
   args+=['-DFREEINK_DEVICE_X4PRO=1','-DCROSSPOINT_VERSION="v1.5.0-230-g252758d3"',str(r/'src/network/OtaUpdater.cpp')]
  elif name=='JsonRecoveryTest':
   args+=['-fsanitize=address,undefined','-I'+str(d/'json_stubs'),'-I'+str(r/'src'),'-I'+str(r/'lib/Serialization'),'-I'+str(r/'.pio/libdeps/x4pro/ArduinoJson/src'),str(r/'lib/Serialization/PersistableStore.cpp'),str(r/'src/HardcoverCredentialStore.cpp')]
  else: args+=['-I'+str(d/'stubs')]
  subprocess.run(args+['-o',exe],check=True)
  subprocess.run([exe],check=True)
  if name=='OtaPolicyTest':
   symbols=subprocess.check_output(['nm','-u',exe],text=True)
   assert not any(x in symbols for x in ('esp_ota','esp_partition','fetchUrl'))
