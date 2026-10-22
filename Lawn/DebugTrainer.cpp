#include "DebugTrainer.h"
#include "../LawnApp.h"
#include "../GameConstants.h"
#include "Board.h"
#include "Plant.h"
#include "Zombie.h"
#include "SeedPacket.h"
#include "LawnCommon.h"
#include "System/PlayerInfo.h"
#include "Widget/ChallengeScreen.h"
#include <windows.h>
#include <filesystem>
#include <deque>
#include <string>
#include <algorithm>
#include <cwchar>
#include <chrono>

namespace {
enum : int {
    Open = 49000, SunEdit, SunApply, CoinsEdit, CoinsApply, LevelEdit, LevelApply,
    Jump, UnlockPlants, UnlockModes, UnlockTrophies, UnlockAchievements, Reset,
    InfiniteSun, EasyPlant, Immortal, Freeze, ClearZombies, Win, NextWave,
    PlantType, ZombieType, Row, Column, SpawnPlant, SpawnZombie, Status, Refresh,
    Backup, Pause, Speed, PageBattle, PageProgress, PageSpawn
};
LawnApp* app = nullptr;
HWND window = nullptr, gameWindow = nullptr;
WNDPROC previousProc = nullptr;
std::deque<int> actions;
bool infiniteSun = false, immortal = false, freezeZombies = false;
int speed = 1;
ULONGLONG lastRefresh = 0;
std::wstring lastAction = L"就绪。进度操作自动备份；战斗开关不保存。";
struct Control { HWND handle; int page; };
std::vector<Control> controls;

void StatusText(const std::wstring& text) {
    lastAction = text;
    if (window) SetWindowTextW(GetDlgItem(window, Status), text.c_str());
}
HWND Add(const wchar_t* cls, const wchar_t* text, DWORD style, int id,
    int x, int y, int w, int h, int page) {
    HWND control = CreateWindowExW(cls == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
        cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    controls.push_back({control, page});
    return control;
}
void Button(const wchar_t* text, int id, int x, int y, int w, int page) {
    Add(L"BUTTON", text, BS_PUSHBUTTON | WS_TABSTOP, id, x,y,w,30,page);
}
void Label(const wchar_t* text, int x, int y, int w, int page) {
    Add(L"STATIC", text, 0, 0, x,y,w,24,page);
}
void Edit(int id, const wchar_t* value, int x, int y, int page) {
    HWND edit = Add(L"EDIT", value, ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, id,x,y,110,26,page);
    SendMessageW(edit, EM_SETLIMITTEXT, 8, 0);
}
void Page(int page) {
    for (const auto& c : controls)
        ShowWindow(c.handle, c.page == 0 || c.page == page ? SW_SHOW : SW_HIDE);
}
bool Number(int id, int min, int max, int& value) {
    wchar_t buffer[64]{};
    GetWindowTextW(GetDlgItem(window,id),buffer,64);
    wchar_t* end = nullptr;
    long n = wcstol(buffer,&end,10);
    if (!buffer[0] || *end || n < min || n > max) {
        StatusText(L"数值无效：允许范围 " + std::to_wstring(min) + L"–" + std::to_wstring(max));
        return false;
    }
    value = static_cast<int>(n);
    return true;
}
void RefreshValues() {
    if (!window || !app->mPlayerInfo) return;
    auto* p = app->mPlayerInfo;
    std::wstring text = L"冒险进度 " + std::to_wstring((p->mLevel-1)/10+1) + L"-" +
        std::to_wstring((p->mLevel-1)%10+1) + L"  周目 " + std::to_wstring(p->mFinishedAdventure) +
        L"  金币 " + std::to_wstring(p->mCoins*10);
    if (app->mBoard) text += L"\r\n当前关卡 " + std::to_wstring(app->mBoard->mLevel) +
        L"  阳光 " + std::to_wstring(app->mBoard->mSunMoney) + L"  波次 " +
        std::to_wstring(app->mBoard->mCurrentWave) + L"/" + std::to_wstring(app->mBoard->mNumWaves);
    else text += L"\r\n当前不在关卡中。";
    SetWindowTextW(GetDlgItem(window,Refresh),text.c_str());
}
void CreateControls() {
    Button(L"战斗调试",PageBattle,16,12,150,0);
    Button(L"进度 / 解锁",PageProgress,174,12,150,0);
    Button(L"生成实体",PageSpawn,332,12,150,0);
    Add(L"STATIC",L"",0,Refresh,18,52,575,48,0);
    Label(L"阳光（0–9990）",20,115,180,1); Edit(SunEdit,L"9990",215,110,1);
    Button(L"设置阳光",SunApply,345,108,150,1);
    const wchar_t* checks[] = {L"锁定阳光 9990",L"自由种植（免阳光 / 无冷却 / 升级限制）",L"植物持续回血",L"冻结僵尸移动与攻击"};
    const int ids[] = {InfiniteSun,EasyPlant,Immortal,Freeze};
    for(int i=0;i<4;++i) Add(L"BUTTON",checks[i],BS_AUTOCHECKBOX | WS_TABSTOP,ids[i],20,155+i*34,555,28,1);
    Button(L"清除所有僵尸",ClearZombies,20,310,175,1);
    Button(L"立即通关",Win,205,310,140,1);
    Button(L"提前刷新下一波",NextWave,355,310,175,1);
    Button(L"暂停 / 继续关卡",Pause,20,360,175,1);
    Button(L"速度：1 倍 / 2 倍 / 5 倍",Speed,205,360,325,1);
    Label(L"战斗开关仅本次运行有效；无关卡时不会修改战斗状态。",20,415,565,1);
    Label(L"冒险进度（1–50；1-1 = 1，5-10 = 50）",20,112,550,2);
    Edit(LevelEdit,L"1",20,145,2);
    Button(L"保存进度（返回主菜单）",LevelApply,145,142,220,2);
    Button(L"跳转到该关",Jump,375,142,175,2);
    Label(L"金币（显示值，0–999990；按 10 取整）",20,195,550,2);
    Edit(CoinsEdit,L"999990",20,228,2); Button(L"设置并保存金币",CoinsApply,145,225,220,2);
    Button(L"解锁全部植物 / 卡槽",UnlockPlants,20,280,260,2);
    Button(L"解锁模式入口（冒险通关）",UnlockModes,290,280,260,2);
    Button(L"全部挑战奖杯",UnlockTrophies,20,320,260,2);
    Button(L"解锁全部成就（不弹提示）",UnlockAchievements,290,320,260,2);
    Button(L"立即备份当前存档",Backup,20,375,260,2);
    Button(L"恢复初始进度…",Reset,290,375,260,2);
    Label(L"解锁 / 重置会返回主菜单；所有存档操作先备份到 debug-backups。",20,425,565,2);
    Label(L"植物（按源码 SeedType ID）",20,118,550,3);
    HWND plants=Add(L"COMBOBOX",L"",CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,PlantType,20,148,530,280,3);
    for(int i=0;i<48;++i) {
        auto name=Plant::GetNameString(static_cast<SeedType>(i),SEED_NONE);
        std::string label=std::to_string(i)+" - "+name;
        int size=MultiByteToWideChar(CP_UTF8,0,label.c_str(),-1,nullptr,0);
        std::wstring wide(size,L'\0'); MultiByteToWideChar(CP_UTF8,0,label.c_str(),-1,wide.data(),size);
        SendMessageW(plants,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide.c_str()));
    }
    SendMessageW(plants,CB_SETCURSEL,0,0);
    Label(L"僵尸类型 ID（0–32，特殊 / Boss 类型不提供生成）",20,202,550,3);
    Edit(ZombieType,L"0",20,232,3);
    Label(L"行（1–6，按当前场景校验）",20,285,300,3); Edit(Row,L"3",360,280,3);
    Label(L"列（1–9）",20,325,300,3); Edit(Column,L"5",360,320,3);
    Button(L"生成选中植物",SpawnPlant,20,375,260,3);
    Button(L"从右侧生成僵尸",SpawnZombie,290,375,260,3);
    Label(L"仅在正在游玩的普通战场生效；会校验地形及格子种植条件。",20,425,565,3);
    Add(L"STATIC",lastAction.c_str(),0,Status,18,475,575,65,0);
    Page(1);
}
LRESULT CALLBACK TrainerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if(msg==WM_CREATE) { window=hwnd; CreateControls(); return 0; }
    if(msg==WM_COMMAND && HIWORD(wp)==BN_CLICKED) {
        int id=LOWORD(wp);
        if(id>=PageBattle && id<=PageSpawn) Page(id-PageBattle+1);
        else actions.push_back(id);
        return 0;
    }
    if(msg==WM_CLOSE) { ShowWindow(hwnd,SW_HIDE); return 0; }
    if(msg==WM_DESTROY) { window=nullptr; controls.clear(); return 0; }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
void Show() {
    if (!window) {
        WNDCLASSW cls{}; cls.lpfnWndProc=TrainerProc; cls.hInstance=GetModuleHandleW(nullptr);
        cls.lpszClassName=L"PVZ.DebugTrainer"; cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
        cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
        RegisterClassW(&cls);
        RECT r{0,0,610,550}; AdjustWindowRectEx(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,WS_EX_APPWINDOW);
        CreateWindowExW(WS_EX_APPWINDOW,cls.lpszClassName,L"PVZ 内置调试修改器",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
            CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,cls.hInstance,nullptr);
    }
    ShowWindow(window,SW_SHOW); SetForegroundWindow(window); RefreshValues();
}
LRESULT CALLBACK GameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WNDPROC old=previousProc;
    if((msg==WM_COMMAND && LOWORD(wp)==Open) || (msg==WM_KEYDOWN && wp==VK_F12)) {
        if(!(msg==WM_KEYDOWN && (lp & (1ll<<30)))) actions.push_back(Open);
        return 0;
    }
    if(msg==WM_NCDESTROY) { gameWindow=nullptr; previousProc=nullptr; }
    return CallWindowProcW(old,hwnd,msg,wp,lp);
}
bool SaveBackup() {
    namespace fs=std::filesystem;
    std::error_code error;
    fs::path root=fs::u8path(Sexy::GetAppDataFolder());
    auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
    auto target=root/"debug-backups"/std::to_string(stamp);
    fs::create_directories(target,error);
    if(!error && fs::exists(root/"userdata")) fs::copy(root/"userdata",target/"userdata",fs::copy_options::recursive,error);
    if(error) { StatusText(L"备份失败，操作已取消："+std::to_wstring(error.value())); return false; }
    StatusText(L"备份完成："+target.wstring()); return true;
}
void LeaveLevel() {
    app->PrepareForDebugTransition();
}
void ResetProgress() {
    PlayerInfo* p=app->mPlayerInfo;
    for(int i=0;i<NUM_GAME_MODES;++i) app->EraseFile(GetSavedGameName(static_cast<GameMode>(i),p->mId));
    p->Reset();
    infiniteSun=immortal=freezeZombies=app->mEasyPlantingCheat=false;
    speed=1;
    for(int flag : {InfiniteSun,EasyPlant,Immortal,Freeze})
        SendDlgItemMessageW(window,flag,BM_SETCHECK,BST_UNCHECKED,0);
}
void Execute(int id) {
    if(id==Open) {
        if(app->mTitleScreen || !app->mPlayerInfo) return;
        Show(); return;
    }
    if(!app->mPlayerInfo || app->mTitleScreen || app->GetDialogCount()>0) {
        StatusText(L"请先完成加载、选择用户并关闭游戏内对话框。"); return;
    }
    PlayerInfo* p=app->mPlayerInfo; Board* b=app->mBoard;
    int value=0;
    if(id==Backup) { SaveBackup(); return; }
    if(id==SunApply) {
        if(!b) { StatusText(L"请先进入关卡。"); return; }
        if(!Number(SunEdit,0,9990,value)) return;
        b->mSunMoney=value;
    } else if(id==InfiniteSun || id==EasyPlant || id==Immortal || id==Freeze) {
        bool checked=SendDlgItemMessageW(window,id,BM_GETCHECK,0,0)==BST_CHECKED;
        if(id==InfiniteSun) infiniteSun=checked;
        if(id==EasyPlant) app->mEasyPlantingCheat=checked;
        if(id==Immortal) immortal=checked;
        if(id==Freeze) freezeZombies=checked;
    } else if(id==Speed) speed=speed==1?2:speed==2?5:1;
    else if(id==CoinsApply || id==LevelApply || id==Jump || (id>=UnlockPlants && id<=Reset)) {
        if(id==CoinsApply && !Number(CoinsEdit,0,999990,value)) return;
        if((id==LevelApply || id==Jump) && !Number(LevelEdit,1,50,value)) return;
        if(id==Reset && MessageBoxW(window,L"将当前用户恢复为初始进度，并清除该用户的关卡续玩存档。\n会先自动备份 userdata。确定继续？",L"恢复初始进度",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES) return;
        if(!SaveBackup()) return;
        if(id!=CoinsApply) LeaveLevel();
        if(id==CoinsApply) p->mCoins=value/10;
        if(id==LevelApply || id==Jump) p->mLevel=value;
        if(id==UnlockPlants || id==UnlockModes || id==UnlockTrophies) {
            p->mFinishedAdventure=std::max(1,p->mFinishedAdventure);
            p->mHasUnlockedMinigames=p->mHasUnlockedPuzzleMode=p->mHasUnlockedSurvivalMode=1;
        }
        if(id==UnlockPlants) {
            for(int i=STORE_ITEM_PLANT_GATLINGPEA;i<=STORE_ITEM_PLANT_IMITATER;++i) p->mPurchases[i]=1;
            p->mPurchases[STORE_ITEM_PACKET_UPGRADE]=4;
        }
        if(id==UnlockTrophies) for(int i=0;i<NUM_CHALLENGE_MODES;++i) {
            const auto& def=gChallengeDefs[i];
            if(def.mPage==CHALLENGE_PAGE_LIMBO || app->IsSurvivalEndless(def.mChallengeMode) ||
                app->IsEndlessScaryPotter(def.mChallengeMode) || app->IsEndlessIZombie(def.mChallengeMode)) continue;
            int flags=app->IsSurvivalNormal(def.mChallengeMode)?SURVIVAL_NORMAL_FLAGS:
                app->IsSurvivalHard(def.mChallengeMode)?SURVIVAL_HARD_FLAGS:1;
            p->mChallengeRecords[i]=std::max(p->mChallengeRecords[i],flags);
        }
        if(id==UnlockAchievements) for(int i=0;i<20;++i) p->mEarnedAchievements[i]=p->mShownAchievements[i]=true;
        if(id==Reset) {
            ResetProgress();
        }
        app->WriteCurrentUserConfig();
        if(id==Jump) app->PreNewGame(GAMEMODE_ADVENTURE,false);
        else if(id!=CoinsApply) app->ShowGameSelector();
        StatusText(L"进度已保存；修改前存档位于 debug-backups。"); RefreshValues(); return;
    } else {
        if(!b || app->mGameScene!=SCENE_PLAYING) { StatusText(L"此操作需要进入正在游玩的关卡。"); return; }
        if(id==ClearZombies) { Zombie* z=nullptr; while(b->IterateZombies(z)) z->DieNoLoot(); }
        if(id==Win) b->mLevelComplete=true;
        if(id==NextWave) {
            if(b->mCurrentWave>=b->mNumWaves) { StatusText(L"已到最后一波。"); return; }
            b->mZombieCountDown=1;
        }
        if(id==Pause) b->mPaused=!b->mPaused;
        if(id==SpawnPlant || id==SpawnZombie) {
            int row,col;
            if(!Number(Row,1,6,row) || !Number(Column,1,9,col)) return;
            if(b->mPlantRow[row-1]==PLANTROW_DIRT) { StatusText(L"当前场景的该行不可用。"); return; }
            if(id==SpawnPlant) {
                int type=static_cast<int>(SendDlgItemMessageW(window,PlantType,CB_GETCURSEL,0,0));
                if(type<0 || type>=48) return;
                if(b->CanPlantAt(col-1,row-1,static_cast<SeedType>(type))!=PLANTING_OK) { StatusText(L"该位置不满足植物种植条件。"); return; }
                Plant::PreloadPlantResources(static_cast<SeedType>(type));
                b->AddPlant(col-1,row-1,static_cast<SeedType>(type),SEED_NONE);
            } else {
                if(!Number(ZombieType,0,32,value)) return;
                if(value==ZOMBIE_BOSS || value==ZOMBIE_BOBSLED || value==ZOMBIE_BACKUP_DANCER || value==ZOMBIE_BUNGEE) { StatusText(L"该类型需要专用场景或生成流程。"); return; }
                if((value==ZOMBIE_SNORKEL || value==ZOMBIE_DOLPHIN_RIDER) && b->mPlantRow[row-1]!=PLANTROW_POOL) { StatusText(L"此僵尸只能生成在水路。"); return; }
                Zombie::PreloadZombieResources(static_cast<::ZombieType>(value));
                b->AddZombieInRow(static_cast<::ZombieType>(value),row-1,b->mCurrentWave);
            }
        }
    }
    StatusText(L"已应用。当前速度 "+std::to_wstring(speed)+L" 倍。"); RefreshValues();
}
}

void UpdateDebugTrainer(LawnApp* theApp) {
    app=theApp;
    if(app->mHWnd && gameWindow!=app->mHWnd) {
        gameWindow=app->mHWnd;
        previousProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(gameWindow,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(GameProc)));
    }
    while(!actions.empty()) { int id=actions.front(); actions.pop_front(); Execute(id); }
    if(app->mBoard && app->mGameScene==SCENE_PLAYING) {
        Board* b=app->mBoard;
        if(infiniteSun) b->mSunMoney=9990;
        if(immortal) { Plant* p=nullptr; while(b->IteratePlants(p)) if(p->mPlantHealth>0) p->mPlantHealth=p->mPlantMaxHealth; }
        if(freezeZombies) { Zombie* z=nullptr; while(b->IterateZombies(z)) z->mIceTrapCounter=std::max(z->mIceTrapCounter,10); }
    }
    if(window && static_cast<DWORD>(GetTickCount()-lastRefresh)>500) { RefreshValues(); lastRefresh=GetTickCount(); }
}

int DebugTrainerSpeed() { return speed; }

void DestroyDebugTrainer() {
    if(window) DestroyWindow(window);
    if(gameWindow && previousProc) {
        SetWindowLongPtrW(gameWindow,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(previousProc));
    }
    gameWindow=nullptr; previousProc=nullptr; app=nullptr; actions.clear();
}

void OpenDebugTrainer() { actions.push_back(Open); }
