#ifndef __NEWOPTIONSDIALOG_H__
#define __NEWOPTIONSDIALOG_H__

#include "widget/Dialog.h"
#include "widget/SliderListener.h"
#include "widget/CheckboxListener.h"
#include "widget/ListListener.h"

class LawnApp;
class LawnStoneButton;
class NewLawnButton;
namespace Sexy
{
    class Slider;
    class Checkbox;
    class ListWidget;
    enum class DisplayMode;
}

class NewOptionsDialog : public Sexy::Dialog, public Sexy::SliderListener, public Sexy::CheckboxListener, public Sexy::ListListener
{
protected:
    enum
    {
        NewOptionsDialog_Almanac,
        NewOptionsDialog_MainMenu,
        NewOptionsDialog_Restart,
        NewOptionsDialog_Update,
        NewOptionsDialog_MusicVolume,
        NewOptionsDialog_SoundVolume,
        NewOptionsDialog_DisplayMode,
        NewOptionsDialog_HardwareAcceleration,
        NewOptionsDialog_PauseOnLostFocus,
        NewOptionsDialog_PageFirst = 100
    };

public:
    enum SettingsPage { PAGE_AUDIO, PAGE_DISPLAY, PAGE_GAME, NUM_PAGES };

    LawnApp* mApp;
    Sexy::Slider* mMusicVolumeSlider;
    Sexy::Slider* mSfxVolumeSlider;
    Sexy::Checkbox* mHardwareAccelerationCheckbox;
    Sexy::Checkbox* mPauseOnLostFocusCheckbox;
    LawnStoneButton* mDisplayModeButton;
    Sexy::ListWidget* mDisplayModeList;
    LawnStoneButton* mPageButtons[NUM_PAGES];
    SettingsPage mSettingsPage;
    int mDisplayModeSelection;
    bool mDisplayModeOpen;
    bool mGameActionsAvailable[3];
    LawnStoneButton* mAlmanacButton;
    LawnStoneButton* mBackToMainButton;
    LawnStoneButton* mRestartButton;
    NewLawnButton* mBackToGameButton;
    bool mFromGameSelector;

    NewOptionsDialog(LawnApp* theApp, bool theFromGameSelector);
    ~NewOptionsDialog();

    int GetPreferredHeight(int theWidth);
    void AddedToManager(Sexy::WidgetManager* theWidgetManager);
    void RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
    void Resize(int theX, int theY, int theWidth, int theHeight);
    void Draw(Sexy::Graphics* g);
    void SliderVal(int theId, double theVal);
    void CheckboxChecked(int theId, bool checked);
    void ButtonPress(int theId);
    void ButtonDepress(int theId);
    void KeyDown(Sexy::KeyCode theKey);

    void SelectPage(SettingsPage thePage);
    void OpenDisplayModeList();
    void CloseDisplayModeList();
    bool IsDisplayModeAvailable(int theIndex) const;
    Sexy::DisplayMode GetSelectedDisplayMode() const;
    void SyncDisplayMode();
    void ListClicked(int theId, int theIdx, int theClickCount);
    void ListClosed(int theId);
    void ListHiliteChanged(int theId, int theOldIdx, int theNewIdx);
};

#endif
