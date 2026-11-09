#include "Version.h"
#include "Menu.h"
#include "Registry.h"
#include "Persistence.h"
#include "Presets.h"
#include "plugin.h"
#include "CPad.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <set>
#include <thread>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace sc {
namespace {
Menu* active{};
constexpr unsigned short kBlock=0x8000;
constexpr ImVec4 kErrorColor(1.0f,0.45f,0.40f,1.0f);
constexpr ImVec4 kWarningColor(1.0f,0.74f,0.36f,1.0f);
std::string Lower(std::string text) {
    std::transform(text.begin(),text.end(),text.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return text;
}
bool Contains(const std::string& hay,const char* needle) {
    if (!needle || !*needle) return true;
    return Lower(hay).find(Lower(needle))!=std::string::npos;
}
std::string CategoryName(const Mod& mod) {
    return mod.category.empty()?"Uncategorized":mod.category;
}
std::string PageName(const Setting& setting) {
    return setting.page.empty()?"General":setting.page;
}
bool MatchesSetting(const Setting& setting,const char* search) {
    return Contains(setting.label,search)||Contains(setting.id,search)||
        Contains(setting.description,search)||Contains(setting.group,search)||
        Contains(PageName(setting),search);
}
bool SamePath(const std::string& a,const std::string& b) {
    auto full=[](const std::string& path) {
        char buffer[MAX_PATH*2]{};
        const DWORD length=GetFullPathNameA(path.c_str(),sizeof buffer,buffer,nullptr);
        return Lower(length && length<sizeof buffer?std::string(buffer,length):path);
    };
    return full(a)==full(b);
}
/* Shows game-relative paths such as modloader\SleekHUD\presets\x.dat. */
std::string GameRelative(const std::string& path) {
    const auto root=GamePath("");
    if (!root.empty() && path.size()>root.size() &&
        Lower(path.substr(0,root.size()))==Lower(root)) return path.substr(root.size());
    return path;
}
/* Right edge of the content region in window-local coordinates. */
float ContentMaxX() {return ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x;}
ImVec4 Mix(const ImVec4& a,const ImVec4& b,float t) {
    return ImVec4(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,
        a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t);
}
bool SameValue(const Setting& setting,const ScValue& value) {
    switch (setting.type) {
    case SC_FLOAT: return setting.value.real==value.real;
    case SC_STRING: return std::strcmp(setting.value.text,value.text)==0;
    default: return setting.value.integer==value.integer;
    }
}
void ApplyModernStyle(const float* rgb,float background) {
    ImGui::StyleColorsDark();
    auto& style=ImGui::GetStyle();
    style.WindowRounding=8.0f;style.ChildRounding=6.0f;
    style.FrameRounding=5.0f;style.PopupRounding=6.0f;
    style.TabRounding=5.0f;style.TabBarBorderSize=1.0f;
    style.ScrollbarRounding=6.0f;style.GrabRounding=4.0f;
    style.WindowBorderSize=1.0f;style.ChildBorderSize=1.0f;
    style.PopupBorderSize=1.0f;style.FrameBorderSize=0.0f;
    style.WindowPadding=ImVec2(10,8);style.FramePadding=ImVec2(8,4);
    style.ItemSpacing=ImVec2(8,5);style.ItemInnerSpacing=ImVec2(6,4);
    style.CellPadding=ImVec2(6,3);style.IndentSpacing=14.0f;
    style.ScrollbarSize=10.0f;style.GrabMinSize=10.0f;
    style.SeparatorTextBorderSize=1.0f;style.SeparatorTextPadding=ImVec2(10,2);
    style.SeparatorTextAlign=ImVec2(0.0f,0.5f);
    style.WindowTitleAlign=ImVec2(0.0f,0.5f);
    style.SelectableTextAlign=ImVec2(0.0f,0.5f);
    const ImVec4 accent(rgb[0],rgb[1],rgb[2],1.00f);
    const ImVec4 base(background+0.026f,background+0.034f,background+0.048f,0.985f);
    const ImVec4 surface(background+0.050f,background+0.060f,
        background+0.077f,1.0f);
    const ImVec4 raised(background+0.098f,background+0.111f,
        background+0.132f,1.0f);
    const ImVec4 accentHover=Mix(accent,ImVec4(1,1,1,1),0.16f);
    const ImVec4 accentSoft=Mix(surface,accent,0.24f);
    const ImVec4 accentRaised=Mix(raised,accent,0.36f);
    auto& c=style.Colors;
    c[ImGuiCol_Text]=ImVec4(0.92f,0.94f,0.97f,1.00f);
    c[ImGuiCol_TextDisabled]=ImVec4(0.56f,0.61f,0.68f,1.00f);
    c[ImGuiCol_WindowBg]=base;
    c[ImGuiCol_ChildBg]=surface;
    c[ImGuiCol_PopupBg]=ImVec4(background+0.068f,background+0.077f,
        background+0.095f,0.995f);
    c[ImGuiCol_Border]=ImVec4(0.26f,0.30f,0.35f,0.55f);
    c[ImGuiCol_BorderShadow]=ImVec4(0,0,0,0);
    c[ImGuiCol_FrameBg]=raised;
    c[ImGuiCol_FrameBgHovered]=Mix(raised,accent,0.17f);
    c[ImGuiCol_FrameBgActive]=Mix(raised,accent,0.28f);
    c[ImGuiCol_TitleBg]=base;
    c[ImGuiCol_TitleBgActive]=base;
    c[ImGuiCol_TitleBgCollapsed]=base;
    c[ImGuiCol_MenuBarBg]=base;
    c[ImGuiCol_CheckMark]=accentHover;
    c[ImGuiCol_SliderGrab]=accent;
    c[ImGuiCol_SliderGrabActive]=accentHover;
    c[ImGuiCol_Button]=raised;
    c[ImGuiCol_ButtonHovered]=accentRaised;
    c[ImGuiCol_ButtonActive]=accentSoft;
    c[ImGuiCol_Header]=accentSoft;
    c[ImGuiCol_HeaderHovered]=accentRaised;
    c[ImGuiCol_HeaderActive]=accentRaised;
    c[ImGuiCol_Tab]=surface;
    c[ImGuiCol_TabHovered]=accentRaised;
    c[ImGuiCol_TabSelected]=accentSoft;
    c[ImGuiCol_TabSelectedOverline]=accentHover;
    c[ImGuiCol_TabDimmed]=surface;
    c[ImGuiCol_TabDimmedSelected]=accentSoft;
    c[ImGuiCol_TabDimmedSelectedOverline]=accent;
    c[ImGuiCol_Separator]=ImVec4(0.28f,0.32f,0.37f,0.60f);
    c[ImGuiCol_SeparatorHovered]=accentRaised;
    c[ImGuiCol_SeparatorActive]=accent;
    c[ImGuiCol_ResizeGrip]=ImVec4(0,0,0,0);
    c[ImGuiCol_ResizeGripHovered]=accentRaised;
    c[ImGuiCol_ResizeGripActive]=accent;
    c[ImGuiCol_ScrollbarBg]=ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]=ImVec4(0.34f,0.39f,0.45f,0.70f);
    c[ImGuiCol_ScrollbarGrabHovered]=accentRaised;
    c[ImGuiCol_ScrollbarGrabActive]=accent;
    c[ImGuiCol_NavCursor]=accent;
    c[ImGuiCol_TextSelectedBg]=ImVec4(accent.x,accent.y,accent.z,0.32f);
}
/* Small rounded label such as UNSAVED next to the mod title. */
void Badge(const char* text,const ImVec4& color) {
    ImGui::SameLine();
    const ImVec2 size=ImGui::CalcTextSize(text);
    const ImVec2 pad(6,1);
    const float frame=ImGui::GetFrameHeight();
    ImVec2 corner=ImGui::GetCursorScreenPos();
    corner.y+=(frame-size.y-pad.y*2)*0.5f;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(corner,ImVec2(corner.x+size.x+pad.x*2,corner.y+size.y+pad.y*2),
        ImGui::GetColorU32(ImVec4(color.x,color.y,color.z,0.18f)),4.0f);
    draw->AddText(ImVec2(corner.x+pad.x,corner.y+pad.y),ImGui::GetColorU32(color),text);
    ImGui::Dummy(ImVec2(size.x+pad.x*2,frame));
}
}
bool Menu::Initialize(HWND window,IDirect3DDevice9* device) {
    Shutdown(); if (!window||!device) return false;
    device_=device;
    if (subclassed_ && window_ && window_!=window) {
        if (reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window_,GWLP_WNDPROC))==
            &Menu::WndProc)
            SetWindowLongPtrA(window_,GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(previous_));
        subclassed_=false;previous_=nullptr;
    }
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr;io.LogFilename=nullptr;
    char windowsDir[MAX_PATH]{};
    const UINT windowsDirLength=GetWindowsDirectoryA(windowsDir,MAX_PATH);
    titleFont_=nullptr;
    if (windowsDirLength && windowsDirLength<MAX_PATH) {
        const std::string fonts=std::string(windowsDir)+"\\Fonts\\";
        ImFontConfig fontConfig{};
        fontConfig.OversampleH=2;fontConfig.OversampleV=2;
        if (!io.Fonts->AddFontFromFileTTF((fonts+"segoeui.ttf").c_str(),16.0f,
                &fontConfig,io.Fonts->GetGlyphRangesDefault()))
            io.Fonts->AddFontFromFileTTF((fonts+"arial.ttf").c_str(),16.0f,
                &fontConfig,io.Fonts->GetGlyphRangesDefault());
        if (!io.Fonts->Fonts.empty()) {
            titleFont_=io.Fonts->AddFontFromFileTTF((fonts+"segoeuib.ttf").c_str(),16.0f,
                &fontConfig,io.Fonts->GetGlyphRangesDefault());
            if (!titleFont_) titleFont_=io.Fonts->AddFontFromFileTTF(
                (fonts+"arialbd.ttf").c_str(),16.0f,&fontConfig,
                io.Fonts->GetGlyphRangesDefault());
        }
    }
    if (io.Fonts->Fonts.empty()) io.Fonts->AddFontDefault();
    ApplyTheme();
    if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplDX9_Init(device)) {
        if (io.BackendPlatformUserData) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();return false;
    }
    if (!subclassed_ || window_!=window) {
        SetLastError(0);
        auto prev=reinterpret_cast<WNDPROC>(SetWindowLongPtrA(window,GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&Menu::WndProc)));
        if (!prev && GetLastError()) {
            ImGui_ImplDX9_Shutdown();ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();return false;
        }
        previous_=prev;subclassed_=true;
    }
    window_=window; initialized_=true; active=this;
    const auto configPath=GamePath("SleekConfig.ini");
    hotkey_=GetPrivateProfileIntA("menu","hotkey_vk",VK_F10,configPath.c_str());
    if (hotkey_<1||hotkey_>254) hotkey_=VK_F10;
    RegisterOwnSettings();
    return true;
}
void Menu::Shutdown() {
    SetOpen(false);
    ClearIcons();device_=nullptr;
    if (ownMod_) {Registry::Instance().Unregister(ownMod_);ownMod_=0;}
    ownView_=false;
    keyCaptureMod_=0;keyCaptureId_.clear();actions_.clear();
    if (initialized_ && ImGui::GetCurrentContext()) {
        ImGui_ImplDX9_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();
    }
    titleFont_=nullptr;
    initialized_=false;
    /* Keep the subclass across RW resets so later subscribers retain their chain. */
}
void Menu::ApplyTheme() {
    if (ImGui::GetCurrentContext()) ApplyModernStyle(accent_,background_);
}
void Menu::RegisterOwnSettings() {
    ScModDesc mod{};
    mod.size=sizeof mod;mod.required_api=SC_API_VERSION;
    mod.id="sleekconfig.ui";mod.name="SleekConfig";mod.author="pedrowav";
    mod.description="Menu appearance and controls";mod.version=SLEEKCONFIG_VERSION;
    mod.category="System";mod.persistence=SC_EXTERNAL;
    mod.on_change=&Menu::OnOwnChange;mod.user=this;
    if (Registry::Instance().Register(&mod,&ownMod_)!=SC_OK) {ownMod_=0;return;}
    auto add=[&](const char* id,const char* label,const char* page,
                 const char* group,uint32_t type,double low,double high,
                 int integer,float real,const char* description) {
        ScSettingDesc setting{};
        setting.size=sizeof setting;setting.id=id;setting.label=label;
        setting.page=page;setting.group=group;setting.type=type;
        setting.minimum=low;setting.maximum=high;setting.step=0;
        setting.default_value.type=type;setting.default_value.integer=integer;
        setting.default_value.real=real;setting.description=description;
        return Registry::Instance().Add(ownMod_,&setting)==SC_OK;
    };
    if (!add("hotkey","Open menu","Controls","Keyboard",SC_KEY,1,254,
             VK_F10,0,"Click the key button and press a new key.") ||
        !add("drag_speed","Drag speed","Controls","Sliders",SC_FLOAT,0.1,1,0,
             1.0f,"How far a slider moves per pixel dragged. 1 follows the mouse; "
             "lower values make small adjustments easier.") ||
        !add("fine_drag_speed","Fine drag speed (Shift)","Controls","Sliders",SC_FLOAT,
             0.01,0.5,0,0.1f,"Drag speed while Shift is held.") ||
        !add("accent_red","Red","Appearance","Accent color",SC_FLOAT,0,1,0,
             0.38f,"Red channel of the menu accent.") ||
        !add("accent_green","Green","Appearance","Accent color",SC_FLOAT,0,1,0,
             0.69f,"Green channel of the menu accent.") ||
        !add("accent_blue","Blue","Appearance","Accent color",SC_FLOAT,0,1,0,
             0.96f,"Blue channel of the menu accent.") ||
        !add("background","Background brightness","Appearance","Background",SC_FLOAT,
             0.03,0.14,0,0.055f,"Brightness of the menu background.") ||
        !add("inline_descriptions","Show descriptions under options","Appearance","Layout",
             SC_BOOL,0,1,1,0,"Off keeps every option on one line; descriptions stay "
             "available when hovering the option's name.")) {
        Registry::Instance().Unregister(ownMod_);ownMod_=0;
    } else if (hotkey_!=VK_F10) {
        ScValue stored{};
        if (ReadStoredValue("sleekconfig.ui","hotkey",stored)==SC_NOT_FOUND) {
            ScValue fallback{};fallback.type=SC_KEY;fallback.integer=hotkey_;
            Registry::Instance().Publish(ownMod_,"hotkey",&fallback);
        }
    }
}
int SC_CALL Menu::OnOwnChange(void* user,const char* id,const ScValue* value) {
    auto* self=static_cast<Menu*>(user);
    if (!self || !id || !value) return 0;
    if (std::strcmp(id,"hotkey")==0 && value->type==SC_KEY) {
        self->hotkey_=value->integer;self->hotkeyDown_=true;return 1;
    }
    if (std::strcmp(id,"inline_descriptions")==0 && value->type==SC_BOOL) {
        self->inlineDescriptions_=value->integer!=0;return 1;
    }
    if (value->type!=SC_FLOAT) return 0;
    if (std::strcmp(id,"drag_speed")==0) {self->dragSpeed_=value->real;return 1;}
    if (std::strcmp(id,"fine_drag_speed")==0) {self->fineDragSpeed_=value->real;return 1;}
    if (std::strcmp(id,"accent_red")==0) self->accent_[0]=value->real;
    else if (std::strcmp(id,"accent_green")==0) self->accent_[1]=value->real;
    else if (std::strcmp(id,"accent_blue")==0) self->accent_[2]=value->real;
    else if (std::strcmp(id,"background")==0) self->background_=value->real;
    else return 0;
    self->ApplyTheme();return 1;
}
void Menu::DeviceLost() {
    if (initialized_ && ImGui::GetCurrentContext())
        ImGui_ImplDX9_InvalidateDeviceObjects();
}
void Menu::DeviceReset() {
    if (initialized_ && ImGui::GetCurrentContext())
        ImGui_ImplDX9_CreateDeviceObjects();
}
void Menu::SetOpen(bool open) {
    if (open_==open) return;
    if (open) hadClip_=GetClipCursor(&previousClip_)!=0;
    open_=open;
    if (open) StartPresetScan();
    if (!open && blocked_) {
        if (auto* pad=CPad::GetPad(0)) pad->DisablePlayerControls &= ~kBlock;
        blocked_=false;
    }
    if (!open && hadClip_) {
        if (GetForegroundWindow()==window_) ClipCursor(&previousClip_);
        else ClipCursor(nullptr);
        hadClip_=false;
    }
    if (ImGui::GetCurrentContext()) ImGui::GetIO().MouseDrawCursor=open;
}
void Menu::StartPresetScan() {
    if (scan_) return;
    auto job=std::make_shared<PresetScan>();
    scan_=job;
    std::vector<std::string> roots{PresetScanRoot(),PresetDirectory()};
    try {
        // Detached on purpose: the job owns its state, so closing the game
        // never waits on a file walk.
        std::thread([job,roots] {
            job->result=ScanPresets(roots);
            job->done.store(true,std::memory_order_release);
        }).detach();
    } catch (...) {
        job->result=ScanPresets(roots);
        job->done.store(true,std::memory_order_release);
    }
}
void Menu::PollPresetScan() {
    if (!scan_ || !scan_->done.load(std::memory_order_acquire)) return;
    presets_=std::move(scan_->result);
    scan_.reset();presetsScanned_=true;
}
void Menu::BeforeGameProcess() {
    if (!initialized_) return;
    const bool down=(GetAsyncKeyState(hotkey_)&0x8000)!=0;
    if (down&&!hotkeyDown_&&!keyCaptureMod_) SetOpen(!open_);
    hotkeyDown_=down;
    if (!open_) return;
    if (auto* pad=CPad::GetPad(0)) {pad->DisablePlayerControls|=kBlock;blocked_=true;}
    ClipCursor(nullptr);
}
void Menu::BlockGameMouse() {
    if (!open_) return;
    const auto& mouse=CPad::NewMouseControllerState;
    if (ImGui::GetCurrentContext()) {
        if (mouse.wheelUp) ImGui::GetIO().AddMouseWheelEvent(0.0f,1.0f);
        if (mouse.wheelDown) ImGui::GetIO().AddMouseWheelEvent(0.0f,-1.0f);
    }
    CPad::PCTempMouseControllerState={};
    CPad::NewMouseControllerState={};
    CPad::OldMouseControllerState={};
}
void Menu::ProcessActions() {
    auto& registry=Registry::Instance();
    while (!actions_.empty()) {
        const auto action=actions_.front();actions_.pop_front();
        ScResult result=SC_INVALID;
        noticeError_=false;
        switch (action.action) {
        case Action::Save: result=registry.Save(action.mod);break;
        case Action::SaveAll: {
            result=SC_OK;
            std::string firstError;
            size_t failed=0;
            std::vector<ScMod> ids;
            for (const auto& [id,mod]:registry.Mods()) ids.push_back(id);
            for (const auto id:ids) {
                const auto saved=registry.Save(id);
                if (saved!=SC_OK) {if (firstError.empty()) firstError=registry.Error(id);
                    ++failed;result=saved;}
            }
            noticeError_=failed!=0;
            notice_=failed?std::to_string(failed)+" mod(s) failed: "+firstError:
                "Every mod saved to its own .dat";
            continue;
        }
        case Action::Reload: result=registry.Reload(action.mod);break;
        case Action::Reset: result=registry.Reset(action.mod);break;
        case Action::LoadPreset: {
            const auto found=registry.Mods().find(action.mod);
            if (found==registry.Mods().end()) {notice_="Mod is no longer registered";
                noticeError_=true;continue;}
            const auto& mod=found->second;
            std::vector<PresetRow> rows;size_t skipped=0;std::string error;
            if (ReadPreset(action.text,mod.id,rows,skipped,error)!=SC_OK) {
                notice_="Preset not loaded: "+error;noticeError_=true;continue;
            }
            size_t applied=0,unchanged=0,unknown=0,invalid=skipped;
            for (const auto& row:rows) {
                const auto setting=std::find_if(mod.settings.begin(),mod.settings.end(),
                    [&](const Setting& s){return s.id==row.id;});
                if (setting==mod.settings.end()) {++unknown;continue;}
                if (setting->type!=row.type) {++invalid;continue;}
                ScValue value=setting->value;value.type=setting->type;
                if (!DecodeValue(value,row.encoded)) {++invalid;continue;}
                if (SameValue(*setting,value)) {++unchanged;continue;}
                if (registry.Set(action.mod,row.id.c_str(),&value)==SC_OK) ++applied;
                else ++invalid;
            }
            currentPreset_[mod.id]=action.name;
            notice_="Preset \""+action.name+"\" loaded: "+std::to_string(applied)+" changed";
            if (unchanged) notice_+=", "+std::to_string(unchanged)+" already set";
            if (unknown) notice_+=", "+std::to_string(unknown)+" not in this version";
            if (invalid) notice_+=", "+std::to_string(invalid)+" invalid skipped";
            notice_+=applied?". Save mod to keep it.":".";
            continue;
        }
        case Action::SavePreset: {
            const auto found=registry.Mods().find(action.mod);
            if (found==registry.Mods().end()) {notice_="Mod is no longer registered";
                noticeError_=true;continue;}
            std::string path,error;
            if (SavePreset(found->second,action.text,path,error)!=SC_OK) {
                notice_="Preset not saved: "+error;noticeError_=true;continue;
            }
            PresetInfo info;
            if (!ReadPresetHeader(path,info)) {
                notice_="Preset saved but could not be read back";noticeError_=true;continue;
            }
            info.own=true;info.source="Saved by you";
            presets_.erase(std::remove_if(presets_.begin(),presets_.end(),
                [&](const PresetInfo& p){return SamePath(p.path,path);}),presets_.end());
            presets_.push_back(info);
            std::stable_sort(presets_.begin(),presets_.end(),
                [](const PresetInfo& a,const PresetInfo& b) {
                    return a.modId!=b.modId?a.modId<b.modId:Lower(a.name)<Lower(b.name);
                });
            currentPreset_[info.modId]=info.name;
            notice_="Preset saved: "+GameRelative(path);
            continue;
        }
        }
        noticeError_=result!=SC_OK;
        notice_=result==SC_OK?"Done":registry.Error(action.mod);
    }
}
void Menu::DrawPage(ScMod mod,const Mod& owner,const std::string& page,
    const char* search) {
    ImGui::PushID(page.c_str());
    std::vector<std::string> groupParts;
    // 0 closed, 1 tree node pushed, 2 top-level section (ID pushed, no indent).
    std::vector<int> groupOpen;
    auto popGroup=[](int state) {
        if (state==1) ImGui::TreePop();
        else if (state==2) ImGui::PopID();
    };
    auto closeGroups=[&] {
        for (auto it=groupOpen.rbegin();it!=groupOpen.rend();++it) popGroup(*it);
        groupParts.clear();groupOpen.clear();
    };
    const auto& colors=ImGui::GetStyle().Colors;
    for (const auto& setting:owner.settings) {
        if (mod==ownMod_ && (setting.id=="accent_green" ||
                setting.id=="accent_blue")) continue;
        if (PageName(setting)!=page ||
            !(MatchesSetting(setting,search) ||
              (mod==ownMod_ && setting.id=="accent_red" &&
               Contains("accent color rgb red green blue picker",search)))) continue;
        std::vector<std::string> parts;size_t start=0;
        while (start<setting.group.size()) {
            const auto end=setting.group.find('/',start);
            parts.push_back(setting.group.substr(start,end-start));
            if (end==std::string::npos) break;start=end+1;
        }
        size_t same=0;
        while (same<parts.size() && same<groupParts.size() &&
            parts[same]==groupParts[same]) ++same;
        for (size_t i=groupParts.size();i>same;--i) popGroup(groupOpen[i-1]);
        groupParts.resize(same);groupOpen.resize(same);
        for (size_t i=same;i<parts.size();++i) {
            const bool parentOpen=i==0||groupOpen.back()!=0;
            int opened=0;
            if (parentOpen) {
                ImGui::SetNextItemOpen(true,ImGuiCond_FirstUseEver);
                if (i==0) {
                    // Top-level groups read as section headers.
                    if (!groupParts.empty() || ImGui::GetCursorPosY()>ImGui::GetStyle().WindowPadding.y+1)
                        ImGui::Dummy(ImVec2(1,2));
                    ImGui::PushStyleColor(ImGuiCol_Header,Mix(colors[ImGuiCol_ChildBg],
                        colors[ImGuiCol_SliderGrab],0.13f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,Mix(colors[ImGuiCol_ChildBg],
                        colors[ImGuiCol_SliderGrab],0.22f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive,Mix(colors[ImGuiCol_ChildBg],
                        colors[ImGuiCol_SliderGrab],0.28f));
                    if (titleFont_) ImGui::PushFont(titleFont_,0.0f);
                    const bool open=ImGui::TreeNodeEx(parts[i].c_str(),ImGuiTreeNodeFlags_Framed|
                        ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen);
                    if (titleFont_) ImGui::PopFont();
                    ImGui::PopStyleColor(3);
                    // Content stays aligned with the header instead of indenting;
                    // the ID scope keeps nested groups of each section apart.
                    if (open) {ImGui::PushID(parts[i].c_str());opened=2;}
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text,colors[ImGuiCol_SliderGrabActive]);
                    opened=ImGui::TreeNodeEx(parts[i].c_str(),
                        ImGuiTreeNodeFlags_SpanAvailWidth)?1:0;
                    ImGui::PopStyleColor();
                }
            }
            groupParts.push_back(parts[i]);groupOpen.push_back(opened);
        }
        if (groupOpen.empty()||groupOpen.back()!=0) DrawSetting(mod,setting);
    }
    closeGroups();
    ImGui::PopID();
}
void Menu::DrawModList(const std::map<ScMod,Mod>& mods) {
    const auto& style=ImGui::GetStyle();
    std::set<std::string> categories;
    for (const auto& [id,mod]:mods) if (id!=ownMod_) categories.insert(CategoryName(mod));
    ImGui::AlignTextToFramePadding();
    if (titleFont_) ImGui::PushFont(titleFont_,0.0f);
    ImGui::TextColored(style.Colors[ImGuiCol_SliderGrab],"MODS");
    if (titleFont_) ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("%u",static_cast<unsigned>(mods.size()-(ownMod_?1:0)));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##mods-search","Search mods...",modSearch_,sizeof modSearch_);
    if (categories.size()>1) {
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##category-filter",categoryFilter_.empty()?"All categories":
                categoryFilter_.c_str())) {
            if (ImGui::Selectable("All categories",categoryFilter_.empty()))
                categoryFilter_.clear();
            for (const auto& category:categories)
                if (ImGui::Selectable(category.c_str(),categoryFilter_==category))
                    categoryFilter_=category;
            ImGui::EndCombo();
        }
    } else categoryFilter_.clear();
    std::vector<ScMod> visible;
    for (const auto& [id,mod]:mods) {
        if (id==ownMod_) continue;
        const auto category=CategoryName(mod);
        if (!categoryFilter_.empty() && categoryFilter_!=category) continue;
        if (Contains(mod.name,modSearch_)||Contains(mod.id,modSearch_)||
            Contains(mod.author,modSearch_)||
            Contains(category,modSearch_)) visible.push_back(id);
    }
    std::sort(visible.begin(),visible.end(),[&](ScMod a,ScMod b) {
        const auto& left=mods.at(a);const auto& right=mods.at(b);
        const auto leftCategory=CategoryName(left),rightCategory=CategoryName(right);
        return leftCategory==rightCategory?Lower(left.name)<Lower(right.name):
            leftCategory<rightCategory;
    });
    if (!ownView_ && std::find(visible.begin(),visible.end(),selected_)==visible.end()) {
        const auto previous=selected_;
        selected_=visible.empty()?0:visible.front();
        if (selected_!=previous) {settingSearch_[0]=0;notice_.clear();}
    }
    ImGui::Spacing();
    ImGui::BeginChild("mod-list",ImVec2(0,0),ImGuiChildFlags_None);
    const float rowHeight=std::max(34.0f,ImGui::GetFontSize()*2.1f);
    const float iconSize=rowHeight-10;
    const float smallSize=std::max(11.0f,ImGui::GetFontSize()-3.0f);
    for (const auto& category:categories) {
        if (!categoryFilter_.empty() && categoryFilter_!=category) continue;
        const bool hasVisible=std::any_of(visible.begin(),visible.end(),[&](ScMod id) {
            return CategoryName(mods.at(id))==category;
        });
        if (!hasVisible) continue;
        ImGui::PushID(category.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text,style.Colors[ImGuiCol_TextDisabled]);
        ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(0,0,0,0));
        std::string caption=category;
        std::transform(caption.begin(),caption.end(),caption.begin(),
            [](unsigned char c){return static_cast<char>(std::toupper(c));});
        ImGui::PushFont(nullptr,smallSize);
        const bool open=ImGui::TreeNodeEx(caption.c_str(),ImGuiTreeNodeFlags_DefaultOpen|
            ImGuiTreeNodeFlags_NoTreePushOnOpen|ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::PopFont();
        ImGui::PopStyleColor(2);
        if (open) {
            for (const auto id:visible) {
                const auto& mod=mods.at(id);
                if (CategoryName(mod)!=category) continue;
                ImGui::PushID(static_cast<int>(id));
                const bool selected=!ownView_&&selected_==id;
                if (ImGui::Selectable("##entry",selected,0,ImVec2(0,rowHeight))) {
                    ownView_=false;
                    if (selected_!=id) {settingSearch_[0]=0;notice_.clear();}
                    selected_=id;
                }
                const ImVec2 corner=ImGui::GetItemRectMin();
                const ImVec2 edge=ImGui::GetItemRectMax();
                ImDrawList* draw=ImGui::GetWindowDrawList();
                if (selected) draw->AddRectFilled(
                    ImVec2(corner.x,corner.y+5),ImVec2(corner.x+3,edge.y-5),
                    ImGui::GetColorU32(ImGuiCol_SliderGrab),2.0f);
                float textX=corner.x+8;
                if (auto* texture=IconFor(id,mod)) {
                    const auto& icon=icons_.at(id);
                    const float scale=std::min(iconSize/icon.width,iconSize/icon.height);
                    const ImVec2 size(icon.width*scale,icon.height*scale);
                    const ImVec2 imageMin(corner.x+8+(iconSize-size.x)*0.5f,
                        corner.y+(rowHeight-size.y)*0.5f);
                    draw->AddImage(texture,imageMin,
                        ImVec2(imageMin.x+size.x,imageMin.y+size.y));
                    textX=corner.x+14+iconSize;
                }
                const ImVec4 clip(textX,corner.y,edge.x-4,edge.y);
                const float nameY=corner.y+(rowHeight-ImGui::GetFontSize()-smallSize)*0.5f;
                draw->AddText(titleFont_?titleFont_:ImGui::GetFont(),ImGui::GetFontSize(),
                    ImVec2(textX,nameY),ImGui::GetColorU32(ImGuiCol_Text),mod.name.c_str(),
                    nullptr,0.0f,&clip);
                std::string detail=mod.version.empty()?std::string{}:"v"+mod.version;
                if (!mod.author.empty()) detail+=(detail.empty()?"":"  ")+mod.author;
                if (mod.dirty) detail="unsaved  "+detail;
                draw->AddText(ImGui::GetFont(),smallSize,
                    ImVec2(textX,nameY+ImGui::GetFontSize()),
                    ImGui::GetColorU32(mod.dirty?kWarningColor:style.Colors[ImGuiCol_TextDisabled]),
                    detail.c_str(),nullptr,0.0f,&clip);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !mod.description.empty())
                    ImGui::SetTooltip("%s",mod.description.c_str());
                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }
    if (visible.empty()) ImGui::TextDisabled("No matching mods");
    ImGui::EndChild();
}
void Menu::DrawPresetControls(ScMod id,const Mod& mod) {
    const auto& style=ImGui::GetStyle();
    std::vector<const PresetInfo*> matching;
    for (const auto& preset:presets_) if (preset.modId==mod.id) matching.push_back(&preset);
    const auto current=currentPreset_.find(mod.id);
    std::string preview=current!=currentPreset_.end()?current->second:
        (matching.empty()?(presetsScanned_?"No presets":"Presets..."):
            "Choose a preset ("+std::to_string(matching.size())+")");
    const float saveWidth=ImGui::CalcTextSize("Save preset").x+style.FramePadding.x*2;
    const float comboWidth=std::clamp(ImGui::GetContentRegionAvail().x*0.32f,150.0f,250.0f);
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),ContentMaxX()-
        comboWidth-saveWidth-style.ItemSpacing.x));
    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::BeginCombo("##presets",preview.c_str(),ImGuiComboFlags_HeightLarge)) {
        if (scan_ && !presetsScanned_) ImGui::TextDisabled("Searching the modloader folder...");
        if (matching.empty() && presetsScanned_)
            ImGui::TextDisabled("No presets for %s yet", mod.name.c_str());
        for (const auto* preset:matching) {
            ImGui::PushID(preset->path.c_str());
            const bool selected=current!=currentPreset_.end() && current->second==preset->name;
            if (ImGui::Selectable(preset->name.c_str(),selected,ImGuiSelectableFlags_AllowOverlap))
                actions_.push_back({id,Action::LoadPreset,preset->path,preset->name});
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                ImGui::SetTooltip("Load these values into %s\n%s",mod.name.c_str(),
                    GameRelative(preset->path).c_str());
            const float sourceWidth=ImGui::CalcTextSize(preset->source.c_str()).x;
            const float nameWidth=ImGui::CalcTextSize(preset->name.c_str()).x;
            if (nameWidth+sourceWidth+24<ImGui::GetItemRectSize().x) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ContentMaxX()-sourceWidth);
                ImGui::TextDisabled("%s",preset->source.c_str());
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        if (ImGui::Selectable(scan_?"Refreshing...":"Refresh list",false,
                ImGuiSelectableFlags_NoAutoClosePopups)) StartPresetScan();
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        ImGui::SetTooltip("Presets for %s found in any folder under modloader",mod.name.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Save preset")) {
        const std::string name=current!=currentPreset_.end()?current->second:std::string{};
        std::snprintf(presetName_,sizeof presetName_,"%s",name.c_str());
        ImGui::OpenPopup("save-preset");
    }
    if (ImGui::BeginPopup("save-preset")) {
        ImGui::TextUnformatted("Save the current values as a preset");
        ImGui::TextDisabled("Every saved option of %s is stored by its id.",mod.name.c_str());
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(320);
        const bool submitted=ImGui::InputTextWithHint("##preset-name","Preset name",
            presetName_,sizeof presetName_,ImGuiInputTextFlags_EnterReturnsTrue);
        const auto path=PresetPath(mod.id,presetName_);
        const bool exists=!path.empty() && PresetFileExists(path);
        if (!path.empty()) ImGui::TextDisabled("%s",GameRelative(path).c_str());
        if (exists) ImGui::TextColored(kWarningColor,"A preset with this name exists and will be replaced.");
        ImGui::BeginDisabled(path.empty());
        if (ImGui::Button(exists?"Replace":"Save",ImVec2(90,0)) || (submitted && !path.empty())) {
            actions_.push_back({id,Action::SavePreset,presetName_,{}});
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel",ImVec2(90,0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
void Menu::DrawModHeader(ScMod id,const Mod& mod) {
    const auto& style=ImGui::GetStyle();
    ImGui::AlignTextToFramePadding();
    if (titleFont_) ImGui::PushFont(titleFont_,ImGui::GetFontSize()+3.0f);
    ImGui::TextColored(style.Colors[ImGuiCol_SliderGrabActive],"%s",mod.name.c_str());
    if (titleFont_) ImGui::PopFont();
    if (!mod.version.empty()) {
        ImGui::SameLine();ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("v%s",mod.version.c_str());
    }
    if (mod.dirty) Badge("UNSAVED",kWarningColor);
    DrawPresetControls(id,mod);
    if (ownView_) ImGui::TextDisabled("Menu theme, layout and controls");
    else if (!mod.description.empty() || !mod.author.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text,style.Colors[ImGuiCol_TextDisabled]);
        if (!mod.author.empty() && !mod.description.empty())
            ImGui::TextWrapped("%s  |  by %s",mod.description.c_str(),mod.author.c_str());
        else if (!mod.description.empty()) ImGui::TextWrapped("%s",mod.description.c_str());
        else ImGui::TextWrapped("by %s",mod.author.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##settings-search","Search options in this mod...",
        settingSearch_,sizeof settingSearch_);
}
void Menu::DrawFooter(bool hasMod) {
    const ScMod target=ownView_?ownMod_:selected_;
    ImGui::BeginDisabled(!hasMod);
    if (ImGui::Button("Save mod")) actions_.push_back({target,Action::Save});
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        ImGui::SetTooltip("Write this mod's values to SleekConfig/<mod-id>.dat");
    ImGui::SameLine();
    if (ImGui::Button("Save all")) actions_.push_back({0,Action::SaveAll});
    ImGui::SameLine();
    if (ImGui::Button("Restore saved")) actions_.push_back({target,Action::Reload});
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        ImGui::SetTooltip("Discard unsaved changes of this mod");
    ImGui::SameLine();
    if (ImGui::Button("Defaults")) actions_.push_back({target,Action::Reset});
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        ImGui::SetTooltip("Set every option of this mod to its default (not saved yet)");
    ImGui::EndDisabled();
    const auto& mods=Registry::Instance().Mods();
    const auto selectedIt=mods.find(target);
    std::string status;ImVec4 color=ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
    if (selectedIt!=mods.end() && !selectedIt->second.error.empty()) {
        status=selectedIt->second.error;color=kErrorColor;
    } else if (!notice_.empty()) {
        status=notice_;if (noticeError_) color=kErrorColor;
        else color=ImGui::GetStyle().Colors[ImGuiCol_Text];
    } else {
        char hotkeyName[32]{};
        const UINT hotkeyScan=MapVirtualKeyA(static_cast<UINT>(hotkey_),MAPVK_VK_TO_VSC);
        if (!hotkeyScan || !GetKeyNameTextA(static_cast<LONG>(hotkeyScan<<16),
                hotkeyName,sizeof hotkeyName))
            std::snprintf(hotkeyName,sizeof hotkeyName,"VK %d",hotkey_);
        status=std::string(hotkeyName)+" closes  |  Shift+drag fine  |  Ctrl+click types";
    }
    ImGui::SameLine();
    const float x=ImGui::GetCursorPosX()+ImGui::GetStyle().ItemSpacing.x;
    const float maxX=ContentMaxX();
    ImGui::SetCursorPosX(x);
    ImVec2 start=ImGui::GetCursorScreenPos();
    start.y+=ImGui::GetStyle().FramePadding.y;
    ImGui::PushStyleColor(ImGuiCol_Text,color);
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(),start,
        ImVec2(start.x+std::max(10.0f,maxX-x),start.y+ImGui::GetFrameHeight()),
        start.x+std::max(10.0f,maxX-x),status.c_str(),nullptr,nullptr);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(std::max(10.0f,maxX-x),ImGui::GetTextLineHeight()));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) &&
        ImGui::CalcTextSize(status.c_str()).x>maxX-x)
        ImGui::SetTooltip("%s",status.c_str());
}
void Menu::Render() {
    if (!initialized_ || !ImGui::GetCurrentContext()) return;
    if (!open_) return;
    PollPresetScan();
    ImGui_ImplDX9_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(std::min(980.0f,display.x*0.9f),
        std::min(640.0f,display.y*0.85f)),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(display.x*0.5f,display.y*0.5f),ImGuiCond_FirstUseEver,
        ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(720,440),ImVec2(1800,1200));
    bool show=open_;
    if (ImGui::Begin("SleekConfig",&show,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_MenuBar)) {
        auto& registry=Registry::Instance();const auto& mods=registry.Mods();
        for (auto it=icons_.begin();it!=icons_.end();) {
            if (mods.find(it->first)==mods.end()) {
                if (it->second.texture) it->second.texture->Release();
                it=icons_.erase(it);
            } else ++it;
        }
        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem(ownView_?"< Back to mods":"Menu settings")) {
                ownView_=!ownView_;settingSearch_[0]=0;notice_.clear();
            }
            if (ImGui::MenuItem("Rescan presets")) StartPresetScan();
            const std::string credit=std::string("SleekConfig ")+SLEEKCONFIG_VERSION+"  |  by pedrowav";
            const float creditWidth=ImGui::CalcTextSize(credit.c_str()).x;
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),ContentMaxX()-creditWidth));
            ImGui::TextDisabled("%s",credit.c_str());
            ImGui::EndMenuBar();
        }
        const float footer=ImGui::GetFrameHeightWithSpacing()+ImGui::GetStyle().ItemSpacing.y;
        const float sidebar=std::clamp(ImGui::GetContentRegionAvail().x*0.25f,200.0f,260.0f);
        ImGui::BeginChild("mods",ImVec2(sidebar,-footer),ImGuiChildFlags_Borders);
        DrawModList(mods);
        ImGui::EndChild();ImGui::SameLine();
        const ScMod target=ownView_?ownMod_:selected_;
        const auto selectedIt=mods.find(target);
        ImGui::BeginChild("settings",ImVec2(0,-footer),ImGuiChildFlags_Borders);
        if (selectedIt!=mods.end()) {
            const auto& m=selectedIt->second;
            DrawModHeader(target,m);
            std::vector<std::string> pages;
            for (const auto& setting:m.settings) if (
                MatchesSetting(setting,settingSearch_) ||
                (ownView_ && setting.id=="accent_red" &&
                 Contains("accent color rgb red green blue picker",settingSearch_))) {
                const auto page=PageName(setting);
                if (std::find(pages.begin(),pages.end(),page)==pages.end())
                    pages.push_back(page);
            }
            if (pages.empty()) ImGui::TextDisabled("No matching options");
            else {
                ImGui::PushID(static_cast<int>(target));
                if (ImGui::BeginTabBar("mod-pages",ImGuiTabBarFlags_FittingPolicyScroll|
                        ImGuiTabBarFlags_DrawSelectedOverline)) {
                    for (const auto& page:pages) {
                        if (ImGui::BeginTabItem(page.c_str())) {
                            // Only the options scroll; the header and presets stay visible.
                            ImGui::BeginChild("page",ImVec2(0,0),ImGuiChildFlags_None,
                                ImGuiWindowFlags_NoBackground);
                            DrawPage(target,m,page,settingSearch_);
                            ImGui::EndChild();
                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                }
                ImGui::PopID();
            }
        } else ImGui::TextDisabled("No mods registered");
        ImGui::EndChild();
        DrawFooter(selectedIt!=mods.end());
    }
    ImGui::End();SetOpen(show);ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
LRESULT CALLBACK Menu::WndProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    return active?active->Handle(h,m,w,l):DefWindowProcA(h,m,w,l);
}
LRESULT Menu::Handle(HWND h,UINT m,WPARAM w,LPARAM l) {
    if (m==WM_KILLFOCUS) SetOpen(false);
    if (open_ && m==WM_KEYDOWN && keyCaptureMod_) {
        if (w!=VK_ESCAPE && w<255) {
            auto& registry=Registry::Instance();
            bool conflict=static_cast<int>(w)==hotkey_;
            for (const auto& [modId,mod]:registry.Mods())
                for (const auto& s:mod.settings)
                    if (s.type==SC_KEY && (modId!=keyCaptureMod_||s.id!=keyCaptureId_)
                        && s.value.integer==static_cast<int>(w)) conflict=true;
            if (conflict) {notice_="Key already assigned in this mod";noticeError_=true;}
            else {ScValue value{};value.type=SC_KEY;value.integer=static_cast<int>(w);
                registry.Queue(keyCaptureMod_,keyCaptureId_,value);notice_="Key assigned";
                noticeError_=false;}
        } else {notice_="Key capture cancelled";noticeError_=false;}
        keyCaptureMod_=0;keyCaptureId_.clear();return 1;
    }
    if (open_&&ImGui::GetCurrentContext()) {
        if (m==WM_MOUSEWHEEL || m==WM_MOUSEHWHEEL) return 1;
        ImGui_ImplWin32_WndProcHandler(h,m,w,l);
        if ((m>=WM_MOUSEFIRST&&m<=WM_MOUSELAST)||
            (m>=WM_KEYFIRST&&m<=WM_KEYLAST)||m==WM_CHAR) return 1;
    }
    return previous_?CallWindowProcA(previous_,h,m,w,l):DefWindowProcA(h,m,w,l);
}
}
