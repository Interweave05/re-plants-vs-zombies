#include "../Board.h"
#include "../DebugTrainer.h"
#include "GameButton.h"
#include "../Cutscene.h"
#include "Almanac.h"
#include "../LawnCommon.h"
#include "../../LawnApp.h"
#include "../System/Music.h"
#include "../../Resources.h"
#include "NewOptionsDialog.h"
#include "../../ConstEnums.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "widget/Slider.h"
#include "widget/Checkbox.h"
#include "widget/ListWidget.h"
#include "widget/WidgetManager.h"
#include "../../Sexy.TodLib/TodStringFile.h"

using namespace Sexy;

namespace
{
const char* const gSettingsPages[] = {"Sound", "Display", "Game"};
static_assert(sizeof(gSettingsPages) / sizeof(gSettingsPages[0]) == NewOptionsDialog::NUM_PAGES);
const char* const gDisplayModes[] = {"Windowed", "Original Fullscreen", "Borderless Fullscreen"};
const int kModeRowHeight = 34;

void DrawModeButton(Graphics* g, NewOptionsDialog* theDialog, int theWidth, bool isOver)
{
    DrawStoneButton(g, 0, 0, theWidth, kModeRowHeight, theDialog->mDisplayModeOpen, isOver, _S(""));
    TodDrawString(g, gDisplayModes[theDialog->mDisplayModeSelection], 13, 22,
        FONT_DWARVENTODCRAFT12, Color(191, 195, 213), DrawStringJustification::DS_ALIGN_LEFT);
    g->SetColor(Color(142, 209, 68));
    for (int i = 0; i < 5; i++)
    {
        int aY = theDialog->mDisplayModeOpen ? 19 - i : 14 + i;
        g->DrawLine(theWidth - 25 + i, aY, theWidth - 17 - i, aY);
    }
}

class SettingsTabButton : public LawnStoneButton
{
public:
    SettingsTabButton(int theId, ButtonListener* theListener, const char* theLabel) : LawnStoneButton(nullptr, theId, theListener)
    {
        SetLabel(theLabel);
        mHasAlpha = mHasTransparencies = true;
    }
    void Draw(Graphics* g) override
    {
        DrawStoneButton(g, 0, 0, mWidth, mHeight, mInverted || (mIsDown && mIsOver), mInverted || mIsOver, mLabel);
        if (mInverted)
        {
            g->SetColor(Color(119, 179, 57));
            g->DrawLine(17, 29, mWidth - 17, 29);
        }
    }
};

class DisplayModeButton : public LawnStoneButton
{
    NewOptionsDialog* mDialog;
public:
    DisplayModeButton(int theId, NewOptionsDialog* theDialog) : LawnStoneButton(nullptr, theId, theDialog), mDialog(theDialog)
    {
        mHasAlpha = mHasTransparencies = true;
    }
    void Draw(Graphics* g) override { DrawModeButton(g, mDialog, mWidth, mIsOver); }
};

class DisplayModeList : public ListWidget
{
    NewOptionsDialog* mDialog;
public:
    DisplayModeList(int theId, NewOptionsDialog* theDialog) : ListWidget(theId, FONT_DWARVENTODCRAFT12, theDialog), mDialog(theDialog)
    {
        mPriority = 2; // Above the modal options dialog, with normal popup dismissal.
        mHasAlpha = mHasTransparencies = true;
        mItemHeight = kModeRowHeight;
        for (const char* aMode : gDisplayModes)
            AddLine(aMode, false);
    }

    void Draw(Graphics* g) override
    {
        DrawModeButton(g, mDialog, mWidth, true);
        g->SetColor(Color(44, 45, 60));
        g->FillRect(0, kModeRowHeight, mWidth, kModeRowHeight * 3);
        g->SetColor(Color(117, 120, 150));
        g->DrawRect(0, kModeRowHeight, mWidth - 1, kModeRowHeight * 3 - 1);
        for (int i = 0; i < 3; i++)
        {
            int aY = kModeRowHeight * (i + 1);
            bool isAvailable = mDialog->IsDisplayModeAvailable(i);
            if (i == mHiliteIdx && isAvailable)
            {
                g->SetColor(Color(67, 78, 60));
                g->FillRect(2, aY + 1, mWidth - 4, kModeRowHeight - 2);
            }
            Color aText = !isAvailable ? Color(97, 99, 115) :
                (i == mHiliteIdx || i == mDialog->mDisplayModeSelection ? Color(149, 221, 76) : Color(187, 190, 210));
            TodDrawString(g, gDisplayModes[i], 29, aY + 22, FONT_DWARVENTODCRAFT12, aText, DrawStringJustification::DS_ALIGN_LEFT);
            if (i == mDialog->mDisplayModeSelection)
            {
                g->SetColor(Color(149, 221, 76));
                g->DrawLine(10, aY + 17, 14, aY + 21);
                g->DrawLine(14, aY + 21, 21, aY + 12);
            }
        }
    }

    void MouseMove(int /*x*/, int y) override
    {
        int anIndex = y < kModeRowHeight ? -1 : (y - kModeRowHeight) / kModeRowHeight;
        if (anIndex >= 3) anIndex = -1;
        SetHilite(anIndex, true);
        mWidgetManager->mApp->SetCursor(anIndex < 0 || mDialog->IsDisplayModeAvailable(anIndex) ? CURSOR_HAND : CURSOR_POINTER);
    }

    void MouseDown(int /*x*/, int y, int /*theBtnNum*/, int theClickCount) override
    {
        if (theClickCount <= 0) return;
        if (y < kModeRowHeight)
            mDialog->CloseDisplayModeList();
        else
            mDialog->ListClicked(mId, (y - kModeRowHeight) / kModeRowHeight, theClickCount);
    }
};
}

//0x45C050
NewOptionsDialog::NewOptionsDialog(LawnApp* theApp, bool theFromGameSelector) : 
	Dialog(nullptr, nullptr, Dialogs::DIALOG_NEWOPTIONS, true, _S("Options"), _S(""), _S(""), Dialog::BUTTONS_NONE)
{
    mApp = theApp;
    mFromGameSelector = theFromGameSelector;
    mSettingsPage = PAGE_AUDIO;
    mDisplayModeOpen = false;
    mDisplayModeSelection = static_cast<int>(mApp->GetDisplayMode());
    for (int i = 0; i < NUM_PAGES; i++)
        mPageButtons[i] = new SettingsTabButton(NewOptionsDialog_PageFirst + i, this, gSettingsPages[i]);
    mDisplayModeButton = new DisplayModeButton(NewOptionsDialog_DisplayMode, this);
    mDisplayModeList = new DisplayModeList(NewOptionsDialog_DisplayMode, this);
    SetColor(Dialog::COLOR_BUTTON_TEXT, Color(255, 255, 100));
    mAlmanacButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Almanac, this, _S("[VIEW_ALMANAC_BUTTON]"));
    mRestartButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Restart, this, _S("[RESTART_LEVEL_BUTTON]")); // @Patoke: wrong local name
    mBackToMainButton = MakeButton(NewOptionsDialog::NewOptionsDialog_MainMenu, this, _S("[MAIN_MENU_BUTTON]"));
    mDebugTrainerButton = MakeButton(NewOptionsDialog_DebugTrainer, this, _S("Debug Trainer"));

    mBackToGameButton = MakeNewButton(
        Dialog::ID_OK, 
        this, 
        _S("[BACK_TO_GAME]"), 
        nullptr, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON0, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON0, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON2
    );
    mBackToGameButton->mTranslateX = 0;
    mBackToGameButton->mTranslateY = 0;
    mBackToGameButton->mTextOffsetX = -2;
    mBackToGameButton->mTextOffsetY = -5;
    mBackToGameButton->mTextDownOffsetX = 0;
    mBackToGameButton->mTextDownOffsetY = 1;
    mBackToGameButton->SetFont(FONT_DWARVENTODCRAFT36GREENINSET);
    mBackToGameButton->SetColor(ButtonWidget::COLOR_LABEL, Color::White);
    mBackToGameButton->SetColor(ButtonWidget::COLOR_LABEL_HILITE, Color::White);
    mBackToGameButton->mHiliteFont = FONT_DWARVENTODCRAFT36BRIGHTGREENINSET;
    
    mMusicVolumeSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_MusicVolume, this);
    double aMusicVolume = theApp->GetMusicVolume();
    aMusicVolume = std::max(0.0, std::min(1.0, aMusicVolume));
    mMusicVolumeSlider->SetValue(aMusicVolume);

    mSfxVolumeSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_SoundVolume, this);
    mSfxVolumeSlider->SetValue(theApp->GetSfxVolume() / 0.65);

    mHardwareAccelerationCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_HardwareAcceleration, this, theApp->Is3DAccelerated());
    mPauseOnLostFocusCheckbox = MakeNewCheckbox(NewOptionsDialog_PauseOnLostFocus, this, theApp->mPauseOnLostFocus);

    if (mFromGameSelector)
    {
        mRestartButton->SetVisible(false);
        mBackToGameButton->SetLabel(_S("[DIALOG_BUTTON_OK]"));
        if (mApp->HasFinishedAdventure() && !mApp->IsTrialStageLocked())
        {
            mBackToMainButton->SetLabel(_S("[CREDITS]"));
        }
        else
        {
            mBackToMainButton->SetVisible(false);
        }
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE || 
        mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || 
        mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
    {
        mRestartButton->SetVisible(false);
    }
    if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && !mApp->mBoard->mCutScene->IsSurvivalRepick())
    {
        mRestartButton->SetVisible(false);
    }
    if (!mApp->CanShowAlmanac() || 
        mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO || 
        mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
        mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM || 
        mFromGameSelector)
    {
        mAlmanacButton->SetVisible(false);
    }
    mGameActionsAvailable[0] = mAlmanacButton->mVisible;
    mGameActionsAvailable[1] = mRestartButton->mVisible;
    mGameActionsAvailable[2] = mBackToMainButton->mVisible;
    SelectPage(PAGE_AUDIO);
}

//0x45C760、0x45C780
NewOptionsDialog::~NewOptionsDialog()
{
    delete mMusicVolumeSlider;
    delete mSfxVolumeSlider;
    CloseDisplayModeList();
    delete mDisplayModeList;
    delete mDisplayModeButton;
    for (auto aButton : mPageButtons) delete aButton;
    delete mHardwareAccelerationCheckbox;
    delete mPauseOnLostFocusCheckbox;
    delete mAlmanacButton;
    delete mRestartButton;
    delete mBackToMainButton;
    delete mBackToGameButton;
    delete mDebugTrainerButton;
}

//0x45C880
int NewOptionsDialog::GetPreferredHeight(int theWidth)
{
    (void)theWidth;
    return IMAGE_OPTIONS_MENUBACK->mHeight;
}

//0x45C890
void NewOptionsDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager)
{
    Dialog::AddedToManager(theWidgetManager);
    AddWidget(mAlmanacButton);
    AddWidget(mDebugTrainerButton);
    AddWidget(mRestartButton);
    AddWidget(mBackToMainButton);
    AddWidget(mMusicVolumeSlider);
    AddWidget(mSfxVolumeSlider);
    AddWidget(mHardwareAccelerationCheckbox);
    AddWidget(mPauseOnLostFocusCheckbox);
    AddWidget(mDisplayModeButton);
    for (auto aButton : mPageButtons) AddWidget(aButton);
    AddWidget(mBackToGameButton);
}

//0x45C930
void NewOptionsDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
    CloseDisplayModeList();
    Dialog::RemovedFromManager(theWidgetManager);
    RemoveWidget(mAlmanacButton);
    RemoveWidget(mDebugTrainerButton);
    RemoveWidget(mMusicVolumeSlider);
    RemoveWidget(mSfxVolumeSlider);
    RemoveWidget(mDisplayModeButton);
    for (auto aButton : mPageButtons) RemoveWidget(aButton);
    RemoveWidget(mHardwareAccelerationCheckbox);
    RemoveWidget(mPauseOnLostFocusCheckbox);
    RemoveWidget(mBackToMainButton);
    RemoveWidget(mBackToGameButton);
    RemoveWidget(mRestartButton);
}

//0x45C9D0
void NewOptionsDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
    Dialog::Resize(theX, theY, theWidth, theHeight);
    CloseDisplayModeList();
    for (int i = 0; i < NUM_PAGES; i++)
        mPageButtons[i]->Resize(66 + i * 98, 111, 96, 33);
    mMusicVolumeSlider->Resize(199, 195, 135, 40);
    mSfxVolumeSlider->Resize(199, 244, 135, 40);
    mHardwareAccelerationCheckbox->Resize(283, 264, 46, 45);
    mPauseOnLostFocusCheckbox->Resize(283, 157, 46, 45);
    mDisplayModeButton->Resize(85, 202, 252, kModeRowHeight);
    mAlmanacButton->Resize(107, 183, 209, 46);
    mRestartButton->Resize(mAlmanacButton->mX, mAlmanacButton->mY + 43, 209, 46);
    mBackToMainButton->Resize(mRestartButton->mX, mRestartButton->mY + 43, 209, 46);
    mBackToGameButton->Resize(30, 381, mBackToGameButton->mWidth, mBackToGameButton->mHeight);
    SelectPage(mSettingsPage);

}

//0x45CB50
void NewOptionsDialog::Draw(Sexy::Graphics* g)
{
    g->DrawImage(IMAGE_OPTIONS_MENUBACK, 0, 0);

    Sexy::Color aTextColor(107, 109, 145);
    if (mSettingsPage == PAGE_AUDIO)
    {
        TodDrawString(g, _S("VOLUME"), 212, 180, FONT_DWARVENTODCRAFT12, aTextColor, DrawStringJustification::DS_ALIGN_CENTER);
        TodDrawString(g, _S("Music"), 186, 219, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
        TodDrawString(g, _S("Sound FX"), 186, 268, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
        TodDrawString(g, _S("Changes take effect immediately."), 212, 329, FONT_DWARVENTODCRAFT12, aTextColor, DrawStringJustification::DS_ALIGN_CENTER);
    }
    else if (mSettingsPage == PAGE_DISPLAY)
    {
        TodDrawString(g, _S("Display Mode"), 212, 187, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_CENTER);
        TodDrawString(g, _S("3D Acceleration"), 274, 287, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
        const char* aDescription[] = {"Classic 800 x 600 window.", "Original exclusive fullscreen.", "Desktop fullscreen, original 4:3."};
        TodDrawString(g, aDescription[mDisplayModeSelection], 212, 328, FONT_DWARVENTODCRAFT12, aTextColor, DrawStringJustification::DS_ALIGN_CENTER);
        TodDrawString(g, _S("Alt+Enter toggles fullscreen."), 212, 349, FONT_DWARVENTODCRAFT12, aTextColor, DrawStringJustification::DS_ALIGN_CENTER);
    }
    else
    {
        TodDrawString(g, _S("Pause When Unfocused"), 274, 180, FONT_DWARVENTODCRAFT12, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
        if (!mGameActionsAvailable[0] && !mGameActionsAvailable[1] && !mGameActionsAvailable[2])
        {
            TodDrawString(g, _S("Turn off to keep playing"), 212, 284, FONT_DWARVENTODCRAFT12, aTextColor, DrawStringJustification::DS_ALIGN_CENTER);
            TodDrawString(g, _S("when switching to other windows."), 212, 307, FONT_DWARVENTODCRAFT12, aTextColor, DrawStringJustification::DS_ALIGN_CENTER);
        }
    }
}

void NewOptionsDialog::SelectPage(SettingsPage thePage)
{
    CloseDisplayModeList();
    mSettingsPage = thePage;
    for (int i = 0; i < NUM_PAGES; i++)
    {
        mPageButtons[i]->mInverted = i == thePage;
        mPageButtons[i]->MarkDirty();
    }
    mMusicVolumeSlider->SetVisible(thePage == PAGE_AUDIO);
    mSfxVolumeSlider->SetVisible(thePage == PAGE_AUDIO);
    mDisplayModeButton->SetVisible(thePage == PAGE_DISPLAY);
    mHardwareAccelerationCheckbox->SetVisible(thePage == PAGE_DISPLAY);
    mPauseOnLostFocusCheckbox->SetVisible(thePage == PAGE_GAME);
    mDebugTrainerButton->SetVisible(thePage == PAGE_GAME);
    mAlmanacButton->SetVisible(thePage == PAGE_GAME && mGameActionsAvailable[0]);
    mRestartButton->SetVisible(thePage == PAGE_GAME && mGameActionsAvailable[1]);
    mBackToMainButton->SetVisible(thePage == PAGE_GAME && mGameActionsAvailable[2]);
    if (thePage == PAGE_GAME)
    {
        int aY = 211;
        LawnStoneButton* aButtons[] = {mDebugTrainerButton, mAlmanacButton, mRestartButton, mBackToMainButton};
        for (auto aButton : aButtons)
        {
            if (!aButton->mVisible) continue;
            aButton->Resize(107, aY, 209, 40);
            aY += 40;
        }
    }
    if (mWidgetManager) mWidgetManager->SetFocus(this);
    MarkDirty();
}

bool NewOptionsDialog::IsDisplayModeAvailable(int theIndex) const
{
    if (theIndex < 0 || theIndex >= 3) return false;
    if (theIndex == 0) return !mApp->mForceFullscreen;
    // An unavailable exclusive mode must not disable desktop-cooperative borderless mode.
    return theIndex != 1 || !mApp->mForceWindowed;
}

DisplayMode NewOptionsDialog::GetSelectedDisplayMode() const
{
    return static_cast<DisplayMode>(mDisplayModeSelection);
}

void NewOptionsDialog::SyncDisplayMode()
{
    CloseDisplayModeList();
    mDisplayModeSelection = static_cast<int>(mApp->GetDisplayMode());
    mDisplayModeButton->MarkDirty();
    MarkDirty();
}

void NewOptionsDialog::OpenDisplayModeList()
{
    if (!mWidgetManager || mSettingsPage != PAGE_DISPLAY || mDisplayModeOpen) return;
    mWidgetManager->RemovePopupCommandWidget();
    Point aPos = mDisplayModeButton->GetAbsPos();
    // Include the header in the popup so clicking the arrow again closes it.
    mDisplayModeList->Resize(aPos.mX, aPos.mY, mDisplayModeButton->mWidth, kModeRowHeight * 4);
    mDisplayModeList->SetSelect(mDisplayModeSelection);
    mDisplayModeList->SetHilite(mDisplayModeSelection);
    mDisplayModeOpen = true;
    mWidgetManager->SetPopupCommandWidget(mDisplayModeList);
    mDisplayModeButton->MarkDirty();
}

void NewOptionsDialog::CloseDisplayModeList()
{
    if (!mDisplayModeOpen) return;
    if (mApp->mWidgetManager->mPopupCommandWidget == mDisplayModeList)
        mApp->mWidgetManager->RemovePopupCommandWidget();
    else if (mDisplayModeList->mWidgetManager)
        mApp->mWidgetManager->RemoveWidget(mDisplayModeList);
    mDisplayModeOpen = false;
}

void NewOptionsDialog::ListClicked(int /*theId*/, int theIdx, int /*theClickCount*/)
{
    if (!IsDisplayModeAvailable(theIdx)) return;
    mDisplayModeSelection = theIdx;
    CloseDisplayModeList();
    mApp->PlaySample(SOUND_BUTTONCLICK);
    MarkDirty();
}

void NewOptionsDialog::ListClosed(int /*theId*/)
{
    mDisplayModeOpen = false;
    mDisplayModeButton->MarkDirty();
}

void NewOptionsDialog::ListHiliteChanged(int /*theId*/, int /*theOldIdx*/, int /*theNewIdx*/)
{
    mDisplayModeList->MarkDirty();
}

//0x45CF50
void NewOptionsDialog::SliderVal(int theId, double theVal)
{
    switch (theId)
    {
    case NewOptionsDialog::NewOptionsDialog_MusicVolume:
        mApp->SetMusicVolume(theVal);
        mApp->mSoundSystem->RehookupSoundWithMusicVolume();
        break;

    case NewOptionsDialog::NewOptionsDialog_SoundVolume:
        mApp->SetSfxVolume(theVal * 0.65);
        mApp->mSoundSystem->RehookupSoundWithMusicVolume();
        if (!mSfxVolumeSlider->mDragging)
        {
            mApp->PlaySample(SOUND_BUTTONCLICK);
        }
        break;
    }
}

//0x45CFF0
void NewOptionsDialog::CheckboxChecked(int theId, bool checked)
{
    switch (theId)
    {
    case NewOptionsDialog_PauseOnLostFocus:
        mApp->SetPauseOnLostFocus(checked);
        break;

    case NewOptionsDialog::NewOptionsDialog_HardwareAcceleration:
        if (checked)
        {
            if (!mApp->Is3DAccelerationSupported())
            {
                mHardwareAccelerationCheckbox->SetChecked(false, false);
                mApp->DoDialog(
                    Dialogs::DIALOG_INFO,
                    true,
                    _S("Not Supported"),
                    _S( "Hardware Acceleration cannot be enabled on this computer.\n\n"
                        "Your video card does not\n"
                        "meet the minimum requirements\n"
                        "for this game."),
                    _S("OK"),
                    Dialog::BUTTONS_FOOTER
                );
            }
            else if (!mApp->Is3DAccelerationRecommended())
            {
                mApp->DoDialog(
                    Dialogs::DIALOG_INFO,
                    true,
                    _S("Warning"),
                    _S( "Your video card may not fully support this feature.\n\n"
                        "If you experience slower performance, please disable Hardware Acceleration.\n"),
                    _S("OK"),
                    Dialog::BUTTONS_FOOTER
                );
            }
        }
        break;
    }
}

//0x45D290
void NewOptionsDialog::KeyDown(Sexy::KeyCode theKey)
{
    if (mDisplayModeOpen)
    {
        if (theKey == KeyCode::KEYCODE_ESCAPE)
            CloseDisplayModeList();
        else if (theKey == KeyCode::KEYCODE_RETURN || theKey == KeyCode::KEYCODE_SPACE)
            ListClicked(NewOptionsDialog_DisplayMode, mDisplayModeList->mHiliteIdx, 1);
        else if (theKey == KeyCode::KEYCODE_UP || theKey == KeyCode::KEYCODE_DOWN)
        {
            int aStep = theKey == KeyCode::KEYCODE_UP ? -1 : 1;
            int anIndex = mDisplayModeList->mHiliteIdx;
            for (int i = 0; i < 3; i++)
            {
                anIndex = (anIndex + aStep + 3) % 3;
                if (IsDisplayModeAvailable(anIndex)) break;
            }
            mDisplayModeList->SetHilite(anIndex);
        }
        return;
    }
    if (theKey == KeyCode::KEYCODE_LEFT || theKey == KeyCode::KEYCODE_RIGHT || theKey == KeyCode::KEYCODE_TAB)
    {
        int aStep = theKey == KeyCode::KEYCODE_LEFT || (theKey == KeyCode::KEYCODE_TAB && mWidgetManager->mKeyDown[KeyCode::KEYCODE_SHIFT]) ? -1 : 1;
        SelectPage(static_cast<SettingsPage>((mSettingsPage + aStep + NUM_PAGES) % NUM_PAGES));
        return;
    }
    if (mSettingsPage == PAGE_DISPLAY && theKey == KeyCode::KEYCODE_DOWN)
    {
        OpenDisplayModeList();
        return;
    }
    if (mApp->mBoard)
    {
        mApp->mBoard->DoTypingCheck(theKey);
    }

    if (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN)
    {
        Dialog::ButtonDepress(Dialog::ID_OK);
    }
    else if (theKey == KeyCode::KEYCODE_ESCAPE)
    {
        Dialog::ButtonDepress(Dialog::ID_CANCEL);
    }
}

//0x45D2F0
void NewOptionsDialog::ButtonPress(int theId)
{
    (void)theId;
    mApp->PlaySample(SOUND_GRAVEBUTTON);
}

//0x45D310
void NewOptionsDialog::ButtonDepress(int theId)
{
    if (theId == NewOptionsDialog_DebugTrainer)
    {
        mApp->KillNewOptionsDialog();
        OpenDebugTrainer();
        return;
    }
    if (theId >= NewOptionsDialog_PageFirst && theId < NewOptionsDialog_PageFirst + static_cast<int>(NUM_PAGES))
    {
        SelectPage(static_cast<SettingsPage>(theId - NewOptionsDialog_PageFirst));
        return;
    }
    if (theId == NewOptionsDialog_DisplayMode)
    {
        if (mDisplayModeOpen) CloseDisplayModeList();
        else OpenDisplayModeList();
        return;
    }
    CloseDisplayModeList();
    Dialog::ButtonDepress(theId);

    switch (theId)
    {
    case NewOptionsDialog::NewOptionsDialog_Almanac:
    {
        AlmanacDialog* aDialog = mApp->DoAlmanacDialog(SeedType::SEED_NONE, ZombieType::ZOMBIE_INVALID);
        aDialog->WaitForResult(true);
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_MainMenu:
    {
        if (mFromGameSelector)
        {
            mApp->KillNewOptionsDialog();
            mApp->KillGameSelector();
            mApp->ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE, false);
        }
        else if (mApp->mBoard && mApp->mBoard->NeedSaveGame())
        {
            mApp->DoConfirmBackToMain();
        }
        else if (mApp->mBoard && mApp->mBoard->mCutScene && mApp->mBoard->mCutScene->IsSurvivalRepick())
        {
            mApp->DoConfirmBackToMain();
        }
        else
        {
            mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
            mApp->DoBackToMain();
        }
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_Restart:
    {
        if (mApp->mBoard)
        {
            SexyString aDialogTitle;
            SexyString aDialogMessage;
            if (mApp->IsPuzzleMode())
            {
                aDialogTitle = _S("[RESTART_PUZZLE_HEADER]");
                aDialogMessage = _S("[RESTART_PUZZLE_BODY]");
            }
            else if (mApp->IsChallengeMode())
            {
                aDialogTitle = _S("[RESTART_CHALLENGE_HEADER]");
                aDialogMessage = _S("[RESTART_CHALLENGE_BODY]");
            }
            else if (mApp->IsSurvivalMode())
            {
                aDialogTitle = _S("[RESTART_SURVIVAL_HEADER]");
                aDialogMessage = _S("[RESTART_SURVIVAL_BODY]");
            }
            else
            {
                aDialogTitle = _S("[RESTART_LEVEL_HEADER]");
                aDialogMessage = _S("[RESTART_LEVEL_BODY]");
            }

            LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(Dialogs::DIALOG_CONFIRM_RESTART, true, aDialogTitle, aDialogMessage, _S(""), Dialog::BUTTONS_YES_NO);
            aDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[RESTART_LEVEL_BUTTON]"));
            aDialog->mLawnNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_CANCEL]"));
            
            if (aDialog->WaitForResult(true) == Dialog::ID_YES)
            {
                mApp->mMusic->StopAllMusic();
                mApp->mSoundSystem->CancelPausedFoley();
                mApp->KillNewOptionsDialog();
                mApp->mBoardResult = BoardResult::BOARDRESULT_RESTART;
                mApp->mSawYeti = mApp->mBoard->mKilledYeti;
                mApp->PreNewGame(mApp->mGameMode, false);
            }
        }
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_Update:
        mApp->CheckForUpdates();
        break;
    }
}
