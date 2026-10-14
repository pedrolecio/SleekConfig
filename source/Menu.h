#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <deque>
#include <map>
#include "sleekconfig.h"
namespace sc {
class Menu {
public:
    bool Initialize(HWND,IDirect3DDevice9*);
    void Shutdown();
    void Render();
    void BeforeGameProcess();
    void BlockGameMouse();
    void ProcessActions();
    void DeviceLost();
    void DeviceReset();
    bool IsOpen() const { return open_; }
private:
#ifdef SC_TEST
    friend struct MenuTestAccess;
#endif
    static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
    LRESULT Handle(HWND,UINT,WPARAM,LPARAM);
    void SetOpen(bool);
    void RegisterOwnSettings();
    void ApplyTheme();
    static int SC_CALL OnOwnChange(void*,const char*,const ScValue*);
    void DrawSetting(ScMod,const struct Setting&);
    void DrawPage(ScMod,const struct Mod&,const std::string&,const char*);
    IDirect3DTexture9* IconFor(ScMod,const struct Mod&);
    void ClearIcons();
    struct IconEntry { std::string path; IDirect3DTexture9* texture{}; int width{},height{}; };
    std::map<ScMod,IconEntry> icons_;
    IDirect3DDevice9* device_{};
    HWND window_{};
    WNDPROC previous_{};
    bool subclassed_{},initialized_{},open_{},blocked_{},hotkeyDown_{};
    RECT previousClip_{};
    bool hadClip_{};
    int hotkey_{VK_F10};
    float accent_[3]{0.38f,0.69f,0.96f};
    float pickerColor_[3]{0.38f,0.69f,0.96f};
    float background_{0.055f};
    ScMod ownMod_{};
    bool ownView_{};
    ScMod selected_{};
    ScMod keyCaptureMod_{};
    std::string keyCaptureId_;
    char modSearch_[128]{},settingSearch_[128]{};
    std::string categoryFilter_;
    std::string notice_;
    enum class Action { Save, SaveAll, Reload, Reset };
    struct PendingAction { ScMod mod; Action action; };
    std::deque<PendingAction> actions_;
};
}
