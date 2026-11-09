#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <atomic>
#include <string>
#include <deque>
#include <map>
#include <memory>
#include <vector>
#include "sleekconfig.h"
#include "Presets.h"
struct ImFont;
namespace sc {
struct Setting;
/* Keyboard/wheel step sizes and display precision for a numeric slider. */
struct SliderScale { double step{},fine{},coarse{}; int decimals{}; };
SliderScale ScaleFor(const Setting&);
double QuantizeSlider(double value,const Setting&,const SliderScale&);
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
    void DrawRowLabel(const struct Setting&,float width,bool clickable,bool* toggled);
    bool FineSlider(const struct Setting&,double& value);
    void DrawPage(ScMod,const struct Mod&,const std::string&,const char*);
    void DrawModList(const std::map<ScMod,struct Mod>&);
    void DrawModHeader(ScMod,const struct Mod&);
    void DrawPresetControls(ScMod,const struct Mod&);
    void DrawFooter(bool hasMod);
    void StartPresetScan();
    void PollPresetScan();
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
    float dragSpeed_{1.0f},fineDragSpeed_{0.1f};
    bool inlineDescriptions_{true};
    struct SliderDrag { unsigned id{}; double startValue{}; float startX{}; bool fine{},moved{}; };
    SliderDrag sliderDrag_{};
#ifdef SC_TEST
    float sliderRect_[4]{}; // last drawn slider frame, for tests
#endif
    ImFont* titleFont_{};
    ScMod ownMod_{};
    bool ownView_{};
    ScMod selected_{};
    ScMod keyCaptureMod_{};
    std::string keyCaptureId_;
    char modSearch_[128]{},settingSearch_[128]{};
    std::string categoryFilter_;
    std::string notice_;
    bool noticeError_{};
    // Presets: a background scan of the modloader tree feeds presets_.
    struct PresetScan { std::atomic<bool> done{false}; std::vector<PresetInfo> result; };
    std::shared_ptr<PresetScan> scan_;
    std::vector<PresetInfo> presets_;
    bool presetsScanned_{};
    std::map<std::string,std::string> currentPreset_; // mod id -> last loaded/saved name
    char presetName_[kPresetNameMax+1]{};
    enum class Action { Save, SaveAll, Reload, Reset, LoadPreset, SavePreset };
    struct PendingAction { ScMod mod; Action action; std::string text; std::string name; };
    std::deque<PendingAction> actions_;
};
}
