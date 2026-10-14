#include "Version.h"
#include "Menu.h"
#include "Registry.h"
#include "Persistence.h"
#include "plugin.h"
#include "CPad.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <set>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace sc {
namespace {
Menu* active{};
constexpr unsigned short kBlock=0x8000;
bool Contains(const std::string& hay,const char* needle) {
    if (!needle || !*needle) return true;
    std::string h=hay,n=needle;
    std::transform(h.begin(),h.end(),h.begin(),[](unsigned char c){return std::tolower(c);});
    std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return std::tolower(c);});
    return h.find(n)!=std::string::npos;
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
ImVec4 Mix(const ImVec4& a,const ImVec4& b,float t) {
    return ImVec4(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,
        a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t);
}
void ApplyModernStyle(const float* rgb,float background) {
    ImGui::StyleColorsDark();
    auto& style=ImGui::GetStyle();
    style.WindowRounding=11.0f;style.ChildRounding=9.0f;
    style.FrameRounding=7.0f;style.PopupRounding=8.0f;
    style.TabRounding=7.0f;style.TabBarBorderSize=0.0f;
    style.ScrollbarRounding=8.0f;style.GrabRounding=6.0f;
    style.WindowBorderSize=1.0f;style.ChildBorderSize=1.0f;
    style.FrameBorderSize=0.0f;
    style.WindowPadding=ImVec2(13,11);style.FramePadding=ImVec2(9,5);
    style.ItemSpacing=ImVec2(8,7);style.ItemInnerSpacing=ImVec2(7,5);
    style.ScrollbarSize=11.0f;
    const ImVec4 accent(rgb[0],rgb[1],rgb[2],1.00f);
    const ImVec4 surface(background+0.055f,background+0.065f,
        background+0.082f,1.0f);
    const ImVec4 raised(background+0.105f,background+0.118f,
        background+0.139f,1.0f);
    const ImVec4 accentHover=Mix(accent,ImVec4(1,1,1,1),0.16f);
    const ImVec4 accentSoft=Mix(surface,accent,0.26f);
    const ImVec4 accentRaised=Mix(raised,accent,0.38f);
    auto& c=style.Colors;
    c[ImGuiCol_Text]=ImVec4(0.94f,0.96f,0.98f,1.00f);
    c[ImGuiCol_TextDisabled]=ImVec4(0.61f,0.66f,0.72f,1.00f);
    c[ImGuiCol_WindowBg]=ImVec4(background+0.026f,background+0.034f,
        background+0.048f,0.985f);
    c[ImGuiCol_ChildBg]=surface;
    c[ImGuiCol_PopupBg]=ImVec4(background+0.071f,background+0.080f,
        background+0.098f,0.995f);
    c[ImGuiCol_Border]=ImVec4(0.27f,0.31f,0.36f,0.60f);
    c[ImGuiCol_FrameBg]=raised;
    c[ImGuiCol_FrameBgHovered]=Mix(raised,accent,0.19f);
    c[ImGuiCol_FrameBgActive]=accentRaised;
    c[ImGuiCol_TitleBg]=surface;
    c[ImGuiCol_TitleBgActive]=surface;
    c[ImGuiCol_MenuBarBg]=surface;
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
    c[ImGuiCol_Separator]=ImVec4(0.29f,0.33f,0.38f,0.65f);
    c[ImGuiCol_ResizeGrip]=accentSoft;
    c[ImGuiCol_ResizeGripHovered]=accentRaised;
    c[ImGuiCol_ResizeGripActive]=accent;
    c[ImGuiCol_ScrollbarBg]=ImVec4(0.07f,0.09f,0.11f,0.65f);
    c[ImGuiCol_ScrollbarGrab]=ImVec4(0.36f,0.41f,0.47f,0.80f);
    c[ImGuiCol_ScrollbarGrabHovered]=accentRaised;
    c[ImGuiCol_ScrollbarGrabActive]=accent;
    c[ImGuiCol_NavHighlight]=accent;
    c[ImGuiCol_TextSelectedBg]=ImVec4(accent.x,accent.y,accent.z,0.32f);
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
    if (windowsDirLength && windowsDirLength<MAX_PATH) {
        const std::string fonts=std::string(windowsDir)+"\\Fonts\\";
        ImFontConfig fontConfig{};
        fontConfig.OversampleH=2;fontConfig.OversampleV=2;
        if (!io.Fonts->AddFontFromFileTTF((fonts+"segoeui.ttf").c_str(),16.0f,
                &fontConfig,io.Fonts->GetGlyphRangesDefault()))
            io.Fonts->AddFontFromFileTTF((fonts+"arial.ttf").c_str(),16.0f,
                &fontConfig,io.Fonts->GetGlyphRangesDefault());
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
        !add("accent_red","Red","Appearance","Accent color",SC_FLOAT,0,1,0,
             0.38f,"Red channel of the menu accent.") ||
        !add("accent_green","Green","Appearance","Accent color",SC_FLOAT,0,1,0,
             0.69f,"Green channel of the menu accent.") ||
        !add("accent_blue","Blue","Appearance","Accent color",SC_FLOAT,0,1,0,
             0.96f,"Blue channel of the menu accent.") ||
        !add("background","Background brightness","Appearance","Background",SC_FLOAT,
             0.03,0.14,0,0.055f,"Brightness of the menu background.")) {
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
    if (value->type!=SC_FLOAT) return 0;
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
            notice_=failed?std::to_string(failed)+" mod(s) failed: "+firstError:
                "Each mod saved to its own .dat";
            continue;
        }
        case Action::Reload: result=registry.Reload(action.mod);break;
        case Action::Reset: result=registry.Reset(action.mod);break;
        }
        notice_=result==SC_OK?"Done":registry.Error(action.mod);
    }
}
void Menu::DrawPage(ScMod mod,const Mod& owner,const std::string& page,
    const char* search) {
    ImGui::PushID(page.c_str());
    std::vector<std::string> groupParts;
    std::vector<bool> groupOpen;
    auto closeGroups=[&] {
        for (auto it=groupOpen.rbegin();it!=groupOpen.rend();++it)
            if (*it) ImGui::TreePop();
        groupParts.clear();groupOpen.clear();
    };
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
        for (size_t i=groupParts.size();i>same;--i)
            if (groupOpen[i-1]) ImGui::TreePop();
        groupParts.resize(same);groupOpen.resize(same);
        for (size_t i=same;i<parts.size();++i) {
            const bool parentOpen=i==0||groupOpen.back();
            bool opened=false;
            if (parentOpen) {ImGui::SetNextItemOpen(true,ImGuiCond_FirstUseEver);
                opened=ImGui::TreeNodeEx(parts[i].c_str());}
            groupParts.push_back(parts[i]);groupOpen.push_back(opened);
        }
        if (groupOpen.empty()||groupOpen.back()) DrawSetting(mod,setting);
    }
    closeGroups();
    ImGui::PopID();
}
void Menu::Render() {
    if (!initialized_ || !ImGui::GetCurrentContext()) return;
    if (!open_) return;
    ImGui_ImplDX9_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(900,590),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(700,450),ImVec2(1500,1000));
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
            if (ImGui::BeginMenu("SleekConfig")) {
                if (ImGui::MenuItem("Settings")) {
                    ownView_=true;settingSearch_[0]=0;notice_.clear();
                }
                ImGui::EndMenu();
            }
            ImGui::TextDisabled("v%s | by pedrowav", SLEEKCONFIG_VERSION);
            ImGui::EndMenuBar();
        }
        auto selectedIt=mods.find(ownView_?ownMod_:selected_);
        std::set<std::string> categories;
        for (const auto& [id,mod]:mods) if (id!=ownMod_) categories.insert(CategoryName(mod));
        ImGui::BeginChild("mods",ImVec2(245,-77),ImGuiChildFlags_Borders);
        ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_SliderGrab],"MOD LIBRARY");
        ImGui::SameLine();ImGui::TextDisabled("%u",static_cast<unsigned>(mods.size()-(ownMod_?1:0)));
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##mods-search","Search mods...",modSearch_,sizeof modSearch_);
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
            return leftCategory==rightCategory?left.name<right.name:
                leftCategory<rightCategory;
        });
        if (std::find(visible.begin(),visible.end(),selected_)==visible.end()) {
            const auto previous=selected_;
            selected_=visible.empty()?0:visible.front();
            selectedIt=mods.find(ownView_?ownMod_:selected_);
            if (selected_!=previous) {settingSearch_[0]=0;notice_.clear();}
        }
        for (const auto& category:categories) {
            if (!categoryFilter_.empty() && categoryFilter_!=category) continue;
            const bool hasVisible=std::any_of(visible.begin(),visible.end(),[&](ScMod id) {
                return CategoryName(mods.at(id))==category;
            });
            if (!hasVisible) continue;
            ImGui::PushID(category.c_str());
            if (ImGui::CollapsingHeader(category.c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto id:visible) {
                    const auto& mod=mods.at(id);
                    if (CategoryName(mod)!=category) continue;
                    ImGui::PushID(static_cast<int>(id));
                    const bool selected=!ownView_&&selected_==id;
                    if (ImGui::Selectable("##entry",selected,0,
                            ImVec2(0,42))) {
                        ownView_=false;selected_=id;selectedIt=mods.find(id);
                        settingSearch_[0]=0;notice_.clear();
                    }
                    const ImVec2 corner=ImGui::GetItemRectMin();
                    const ImVec2 edge=ImGui::GetItemRectMax();
                    ImDrawList* draw=ImGui::GetWindowDrawList();
                    if (selected) draw->AddRectFilled(
                        ImVec2(corner.x+1,corner.y+6),
                        ImVec2(corner.x+4,edge.y-6),
                        ImGui::GetColorU32(ImGuiCol_SliderGrab),2.0f);
                    float textX=corner.x+7;
                    if (auto* texture=IconFor(id,mod)) {
                        const auto& icon=icons_.at(id);
                        const float scale=std::min(34.0f/icon.width,34.0f/icon.height);
                        const ImVec2 size(icon.width*scale,icon.height*scale);
                        const ImVec2 imageMin(corner.x+7+(34-size.x)*0.5f,
                            corner.y+4+(34-size.y)*0.5f);
                        draw->AddImage(texture,imageMin,
                            ImVec2(imageMin.x+size.x,imageMin.y+size.y));
                        textX=corner.x+48;
                    }
                    const ImVec4 clip(textX,corner.y,edge.x-7,edge.y);
                    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),
                        ImVec2(textX,corner.y+2),
                        ImGui::GetColorU32(ImGuiCol_Text),mod.name.c_str(),
                        nullptr,0.0f,&clip);
                    const std::string detail=(mod.author.empty()?"Unknown author":mod.author)+
                        (mod.version.empty()?"":"  |  v"+mod.version);
                    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),
                        ImVec2(textX,corner.y+21),
                        ImGui::GetColorU32(ImGuiCol_TextDisabled),detail.c_str(),
                        nullptr,0.0f,&clip);
                    ImGui::PopID();
                }
            }
            ImGui::PopID();
        }
        if (visible.empty()) ImGui::TextDisabled("No matching mods");
        ImGui::EndChild();ImGui::SameLine();
        ImGui::BeginChild("settings",ImVec2(0,-77),ImGuiChildFlags_Borders);
        if (selectedIt!=mods.end()) {
            const auto& m=selectedIt->second;
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive],"%s",m.name.c_str());
            ImGui::SameLine();if (m.dirty) ImGui::TextDisabled("  |  UNSAVED");
            if (ownView_) ImGui::TextDisabled("Theme and keyboard settings");
            if (!m.description.empty()) ImGui::TextWrapped("%s",m.description.c_str());
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##settings-search","Search settings...",settingSearch_,sizeof settingSearch_);
            std::vector<std::string> pages;
            for (const auto& setting:m.settings) if (
                MatchesSetting(setting,settingSearch_) ||
                (ownView_ && setting.id=="accent_red" &&
                 Contains("accent color rgb red green blue picker",settingSearch_))) {
                const auto page=PageName(setting);
                if (std::find(pages.begin(),pages.end(),page)==pages.end())
                    pages.push_back(page);
            }
            if (pages.empty()) ImGui::TextDisabled("No matching settings");
            else {
                ImGui::PushID(static_cast<int>(ownView_?ownMod_:selected_));
                if (ImGui::BeginTabBar("mod-pages",ImGuiTabBarFlags_FittingPolicyScroll)) {
                    for (const auto& page:pages) {
                        if (ImGui::BeginTabItem(page.c_str())) {
                            DrawPage(ownView_?ownMod_:selected_,m,page,settingSearch_);
                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                }
                ImGui::PopID();
            }
        } else ImGui::TextDisabled("No mods registered");
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::BeginDisabled(selectedIt==mods.end());
        if (ImGui::Button("Save mod",ImVec2(105,0)))
            actions_.push_back({ownView_?ownMod_:selected_,Action::Save});
        ImGui::SameLine();
        if (ImGui::Button("Save all",ImVec2(105,0)))
            actions_.push_back({0,Action::SaveAll});
        ImGui::SameLine();
        if (ImGui::Button("Restore saved",ImVec2(125,0)))
            actions_.push_back({ownView_?ownMod_:selected_,Action::Reload});
        ImGui::SameLine();
        if (ImGui::Button("Defaults",ImVec2(95,0)))
            actions_.push_back({ownView_?ownMod_:selected_,Action::Reset});
        ImGui::EndDisabled();
        ImGui::SameLine();ImGui::TextDisabled("SleekConfig/*.dat");
        char hotkeyName[32]{};
        const UINT hotkeyScan=MapVirtualKeyA(static_cast<UINT>(hotkey_),MAPVK_VK_TO_VSC);
        if (!hotkeyScan || !GetKeyNameTextA(static_cast<LONG>(hotkeyScan<<16),
                hotkeyName,sizeof hotkeyName))
            std::snprintf(hotkeyName,sizeof hotkeyName,"VK %d",hotkey_);
        ImGui::TextDisabled("Controls: %s menu  |  Mouse select  |  Tab navigate  |  SleekConfig > Settings",hotkeyName);
        if (selectedIt!=mods.end()&&!selectedIt->second.error.empty())
            ImGui::TextColored(ImVec4(1.0f,0.42f,0.36f,1.0f),"%s",selectedIt->second.error.c_str());
        else if (!notice_.empty()) ImGui::TextWrapped("%s",notice_.c_str());
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
            if (conflict) notice_="Key already assigned in this mod";
            else {ScValue value{};value.type=SC_KEY;value.integer=static_cast<int>(w);
                registry.Queue(keyCaptureMod_,keyCaptureId_,value);notice_="Key assigned";}
        } else notice_="Key capture cancelled";
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
