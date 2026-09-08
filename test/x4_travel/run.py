from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2];d=Path(__file__).parent
with tempfile.TemporaryDirectory() as t:
 exe=t+'/test'
 subprocess.run(['g++','-std=c++17','-DFREEINK_DEVICE_X4PRO=1','-fsanitize=address,undefined','-I'+str(d/'stubs'),'-I'+str(r),str(d/'Test.cpp'),'-o',exe],check=True);subprocess.run([exe],check=True)
main=(r/'src/main.cpp').read_text();hal=(r/'lib/hal/HalPowerManager.cpp').read_text();home=(r/'src/activities/home/HomeActivity.cpp').read_text()
assert main.index('x4pro_sleep::releaseFrontlightHold()')<main.index('frontlightManager.begin()')
assert main.index('frontlightManager.off()',main.index('void enterDeepSleep'))<main.index('activityManager.goToSleep')
assert hal.index('holdFrontlightOff()')<hal.index('powerDownRailsForSleep()')<hal.index('holdPanelMasterRailOff()')<hal.index('freeink::PowerManager::deepSleep();')
assert 'if (pin == 1) continue;' in hal
assert main.index('BoardConfig::holdPowerRails();') < main.index('gpio.begin();')
assert '#else\n  freeink::PowerManager::deepSleepUntilPowerButton();\n#endif' in hal
assert 'enable_timer' not in (r/'lib/hal/X4ProSleep.h').read_text()
assert 'kQuoteDatePollIntervalMs = 60000' in home
assert 'today > 0 && today != renderedQuoteDate' in home
assert home.index('renderedQuoteDate = today;') < home.index('DailyQuote::select(today / 10000, quoteDay)')
# Byte-identical storage, reader, OTA and SDK versus delivered v234.
changed=subprocess.check_output(['git','diff','d94a2890057ccc39f5d7b1d1cdbf43aff6572953','--name-only'],cwd=r,text=True).splitlines()
allowed={'test/x4_improvements/preservation.json','src/main.cpp','lib/hal/HalPowerManager.cpp','lib/hal/X4ProSleep.h','src/activities/settings/SettingsActivity.cpp','src/activities/home/HomeActivity.cpp','src/activities/home/HomeActivity.h','test/daily_quote/DailyQuoteTest.cpp'}
assert all(x in allowed or x.startswith('test/x4_travel/') for x in changed),changed
assert subprocess.check_output(['git','rev-parse','HEAD:freeink-sdk'],cwd=r,text=True).strip()=='8c960bc2a713c2e3df8700b37cf4ef2361548095'
print('PASS wake reinit/sleep integration, unchanged SDK/storage/reader/OTA; non-X4 original sleep branch retained')
