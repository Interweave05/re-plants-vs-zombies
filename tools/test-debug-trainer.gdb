# Run from the configured build directory:
# gdb -q -batch -x ../tools/test-debug-trainer.gdb SexyAppFramework.exe
# Requires the original game resources. Uses a temporary profile ID.
set pagination off
set confirm off
python
import gdb

def number(expr):
    return int(gdb.parse_and_eval(expr))

def call(expr):
    return gdb.execute('call ' + expr, to_string=True)

def message(msg, wp=0, lp=0):
    call(f'Sexy::SexyAppBase::WindowProc(gLawnApp->mHWnd, {msg}, {wp}, {lp})')
    call('gLawnApp->ProcessDeferredMessages(false)')

def click(x, y):
    lp = (y << 16) | x
    message(0x200, 0, lp)
    message(0x201, 1, lp)
    message(0x202, 0, lp)

def physical(x, y):
    return (number(f'gLawnApp->mDDInterface->mPresentationRect.mX + ({x} * gLawnApp->mDDInterface->mPresentationRect.mWidth + 799) / 800'), number(f'gLawnApp->mDDInterface->mPresentationRect.mY + ({y} * gLawnApp->mDDInterface->mPresentationRect.mHeight + 599) / 600'))

def resume():
    gdb.execute('set var gLawnApp->mWidgetManager->mHasFocus = true')
    gdb.execute('set var gLawnApp->mActive = true')
    gdb.execute('continue')

class MenuReady(gdb.Breakpoint):
    def stop(self):
        return number('gLawnApp->mGameSelector->mSelectorState') != number('SELECTOR_OPEN')

class SlideDone(gdb.Breakpoint):
    def stop(self):
        return number('gLawnApp->mGameSelector->mSlideCounter') == 0

gdb.Breakpoint('TitleScreen.cpp:370', temporary=True, internal=True)
gdb.execute('run')
call('gLawnApp->SwitchScreenMode(true, gLawnApp->Is3DAccelerated(), false)')
click(400, 550)
bp = MenuReady('GameSelector::Update', internal=True)
resume()
bp.delete()
assert number('gLawnApp->mGameSelector->mX') == 0

from pathlib import Path
Path('runtime-deps').mkdir(exist_ok=True)
gdb.Breakpoint('LawnApp::UpdateFrames', temporary=True, internal=True)
gdb.execute('continue')
call('gLawnApp->KillGameSelector()')
call('gLawnApp->ProcessSafeDeleteList()')

# Use an isolated profile ID so the user's saves and achievements are untouched.
appdata=Path(gdb.parse_and_eval('Sexy::gAppDataFolder')['_M_dataplus']['_M_p'].string())
test_id=2147483600
while (appdata/'userdata'/('user'+str(test_id)+'.dat')).exists(): test_id-=1
print('TEST_DATA_DIRECTORY',str(appdata))
test_file=appdata/'userdata'/('user'+str(test_id)+'.dat')
assert not test_file.exists(), 'Reserved test profile ID is already in use'
gdb.execute('set $realPlayer = gLawnApp->mPlayerInfo')
gdb.execute('set $testPlayer = (PlayerInfo*)malloc(sizeof(PlayerInfo))')
call('(void*)memcpy($testPlayer, $realPlayer, sizeof(PlayerInfo))')
gdb.execute('set var gLawnApp->mPlayerInfo = $testPlayer')
gdb.execute('set var $testPlayer->mId = '+str(test_id))

def tick():
    try: call('UpdateDebugTrainer(gLawnApp)')
    except:
        gdb.execute('bt')
        raise

def action(id):
    call(f'(long long)SendMessageW($trainer, 273, {id}, 0)')
    tick()

def edit(id,text):
    call(f'(int)SetDlgItemTextA($trainer, {id}, "{text}")')

def check(id,on):
    call(f'(long long)SendDlgItemMessageW($trainer,{id},241,{1 if on else 0},0)')
    action(id)

tick()
assert number('(void*)GetMenu(gLawnApp->mHWnd)') == 0
call('gLawnApp->DoNewOptions(true)')
gdb.execute('set $opts = (NewOptionsDialog*)gLawnApp->GetDialog(DIALOG_NEWOPTIONS)')
call('$opts->SelectPage((NewOptionsDialog::SettingsPage)2)')
assert number('$opts->mDebugTrainerButton->mVisible')
call('gLawnApp->mWidgetManager->MarkAllDirty()')
call('gLawnApp->mWidgetManager->DrawScreen()')
gdb.execute('set $pixels = gLawnApp->mWidgetManager->mImage->GetBits()')
Path('runtime-deps/trainer-game-menu.bgra').write_bytes(bytes(gdb.selected_inferior().read_memory(number('$pixels'),800*600*4)))
call('$opts->ButtonDepress(9)')
tick()
gdb.execute('set $trainer = (HWND)FindWindowA("PVZ.DebugTrainer", (char*)0)')
assert number('$trainer') != 0
assert number('(int)IsWindowVisible($trainer)')
assert not number('gLawnApp->GetDialog(DIALOG_NEWOPTIONS)')
print('PASS: in-game menu opens independent trainer window without a native menu bar')
call('$testPlayer->Reset()')
gdb.execute('set var $testPlayer->mLevel = 3')
call('gLawnApp->MakeNewBoard()')
call('gLawnApp->mBoard->InitLevel()')
call('gLawnApp->StartPlaying()')
edit(49001,'1234'); action(49002)
assert number('gLawnApp->mBoard->mSunMoney') == 1234
edit(49001,'99999'); action(49002)
assert number('gLawnApp->mBoard->mSunMoney') == 1234
check(49013,True)
assert number('gLawnApp->mBoard->mSunMoney') == 9990
check(49014,True)
assert number('gLawnApp->mEasyPlantingCheat')
action(49030)
assert number('DebugTrainerSpeed()') == 2
action(49030)
assert number('DebugTrainerSpeed()') == 5
action(49030)
assert number('DebugTrainerSpeed()') == 1
print('PASS: sun validation, sun lock, free planting and 1/2/5x speeds')
edit(49022,'3'); edit(49023,'5')
plants=number('gLawnApp->mBoard->mPlants.mSize')
action(49024)
assert number('gLawnApp->mBoard->mPlants.mSize') == plants+1
gdb.execute('set $plant = &gLawnApp->mBoard->mPlants.mBlock[0].mItem')
gdb.execute('set var $plant->mPlantHealth = 10')
check(49015,True)
assert number('$plant->mPlantHealth') == number('$plant->mPlantMaxHealth')
edit(49021,'0'); action(49025)
assert number('gLawnApp->mBoard->mZombies.mSize') >= 1
check(49016,True)
action(49017)
print('PASS: plant / zombie spawning, plant healing, zombie freeze and removal')
action(49029)
assert number('gLawnApp->mBoard->mPaused')
action(49029)
assert not number('gLawnApp->mBoard->mPaused')
action(49019)
assert number('gLawnApp->mBoard->mZombieCountDown') == 1
action(49018)
assert number('gLawnApp->mBoard->mLevelComplete')
print('PASS: pause, next wave and level completion')
edit(49003,'123450'); action(49004)
assert number('$testPlayer->mCoins') == 12345
assert test_file.exists()
action(49008)
assert number('gLawnApp->mBoard') == 0
assert number('$testPlayer->mPurchases[0]') == 1
assert number('$testPlayer->mPurchases[21]') == 4
assert number('$testPlayer->mFinishedAdventure') >= 1
action(49009)
assert number('$testPlayer->mHasUnlockedMinigames')
action(49010)
assert number('gLawnApp->GetNumTrophies((ChallengePage)0)') == 10
action(49011)
assert number('$testPlayer->mEarnedAchievements[19]')
assert number('$testPlayer->mShownAchievements[19]')
edit(49005,'12'); action(49006)
assert number('$testPlayer->mLevel') == 12
action(49007)
assert number('gLawnApp->mBoard->mLevel') == 12
print('PASS: saved coins, plant/mode/trophy/achievement unlocks, progress and level jump')
call('(long long)SendMessageW($trainer,16,0,0)')
assert not number('(int)IsWindowVisible($trainer)')
call('(long long)SendMessageW(gLawnApp->mHWnd,256,123,0)')
tick()
assert number('(int)IsWindowVisible($trainer)')
print('PASS: closing hides trainer and F12 reopens it')
call('gLawnApp->SwitchScreenMode(false, gLawnApp->Is3DAccelerated(), true)')
tick()
call('(long long)SendMessageW(gLawnApp->mHWnd,256,123,0)')
tick()
assert number('(int)IsWindowVisible($trainer)')
assert number('(void*)GetMenu(gLawnApp->mHWnd)') == 0
call('gLawnApp->SwitchScreenMode(true, gLawnApp->Is3DAccelerated(), false)')
tick()
print('PASS: trainer survives borderless / windowed transitions')
call('gLawnApp->PrepareForDebugTransition()')
action(49028)
call("'(anonymous namespace)::ResetProgress()'()")
assert number('$testPlayer->mLevel') == 1
assert number('$testPlayer->mFinishedAdventure') == 0
assert number('$testPlayer->mCoins') == 0
assert number('$testPlayer->mPurchases[0]') == 0
assert number('$testPlayer->mNumPottedPlants') == 0
assert not number('$testPlayer->mEarnedAchievements[19]')
assert not number('gLawnApp->mEasyPlantingCheat')
assert number('DebugTrainerSpeed()') == 1
print('PASS: reset restores initial progress, purchases, coins, achievements and runtime toggles')
gdb.execute('set var gLawnApp->mPlayerInfo = $realPlayer')
call('(void)free($testPlayer)')
if test_file.exists(): test_file.unlink()
gdb.execute('set var gLawnApp->mCloseRequest = true')
gdb.execute('continue')
assert gdb.selected_inferior().pid == 0
print('PASS: normal shutdown and original profile restored')
end

