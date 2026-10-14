#include "Menu.h"
#include "Registry.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace sc {
namespace {
bool IsDefault(const Setting& setting) {
    switch (setting.type) {
    case SC_FLOAT: return setting.value.real==setting.initial.real;
    case SC_STRING: return std::strcmp(setting.value.text,setting.initial.text)==0;
    default: return setting.value.integer==setting.initial.integer;
    }
}
bool ResetButton(const char* tooltip="Restore this option's default") {
    const float size=ImGui::GetFrameHeight();
    const bool clicked=ImGui::Button("##reset-default",ImVec2(size,size));
    const auto corner=ImGui::GetItemRectMin();
    const ImVec2 center(corner.x+size*0.5f,corner.y+size*0.5f);
    const float radius=size*0.23f;
    const ImU32 color=ImGui::GetColorU32(ImGuiCol_Text);
    auto* draw=ImGui::GetWindowDrawList();
    draw->PathArcTo(center,radius,-2.4f,2.4f,20);
    draw->PathStroke(color,1.6f);
    const ImVec2 tip(center.x+radius*std::cos(2.4f),center.y+radius*std::sin(2.4f));
    const ImVec2 base(tip.x+std::sin(2.4f)*size*0.15f,
        tip.y-std::cos(2.4f)*size*0.15f);
    const ImVec2 wing(std::cos(2.4f)*size*0.09f,std::sin(2.4f)*size*0.09f);
    draw->AddTriangleFilled(tip,ImVec2(base.x+wing.x,base.y+wing.y),
        ImVec2(base.x-wing.x,base.y-wing.y),color);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",tooltip);
    return clicked;
}
bool ExactSlider(const char* label,ImGuiDataType type,void* value,
    const void* minimum,const void* maximum,const char* format) {
    // SliderScalar already supports Ctrl+click. Route a double-click through
    // its native text-input activation without changing shared ImGui sources.
    auto& context=*ImGui::GetCurrentContext();
    const auto corner=ImGui::GetCursorScreenPos();
    const ImVec2 edge(corner.x+ImGui::CalcItemWidth(),corner.y+ImGui::GetFrameHeight());
    if (!(context.CurrentItemFlags & ImGuiItemFlags_Disabled) &&
        ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(corner,edge) &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        context.NavActivateId=ImGui::GetID(label);
        context.NavActivateFlags=ImGuiActivateFlags_PreferInput;
    }
    const bool changed=ImGui::SliderScalar(label,type,value,minimum,maximum,format,
        ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoRoundToFormat);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Double-click or Ctrl+click to type a value");
    return changed;
}
double SnapValue(double value,const Setting& setting) {
    value=std::clamp(value,setting.minimum,setting.maximum);
    if (setting.step>0) {
        const double maxSteps=std::floor((setting.maximum-setting.minimum)/setting.step+0.00001);
        const double steps=std::clamp(std::round((value-setting.minimum)/setting.step),0.0,maxSteps);
        value=setting.minimum+steps*setting.step;
    }
    return std::clamp(value,setting.minimum,setting.maximum);
}
}
void Menu::DrawSetting(ScMod mod,const Setting& s) {
    if (mod==ownMod_ && (s.id=="accent_green" || s.id=="accent_blue"))
        return;
    if (mod==ownMod_ && s.id=="accent_red") {
        ImGui::PushID("theme-accent");
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Color");
        ImGui::SameLine();
        if (ImGui::ColorButton("##preview",
                ImVec4(accent_[0],accent_[1],accent_[2],1.0f),
                ImGuiColorEditFlags_NoTooltip,ImVec2(48,22))) {
            for (int i=0;i<3;++i) pickerColor_[i]=accent_[i];
            ImGui::OpenPopup("picker");
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to choose the accent color");
        if (ImGui::BeginPopup("picker")) {
            if (ImGui::ColorPicker3("##accent",pickerColor_,
                    ImGuiColorEditFlags_PickerHueWheel |
                    ImGuiColorEditFlags_NoSidePreview)) {
                static constexpr const char* ids[]{
                    "accent_red","accent_green","accent_blue"};
                for (int i=0;i<3;++i) {
                    ScValue value{};value.type=SC_FLOAT;value.real=pickerColor_[i];
                    Registry::Instance().Queue(mod,ids[i],value);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::EndGroup();
        const auto owner=Registry::Instance().Mods().find(mod);
        const bool accentChanged=owner!=Registry::Instance().Mods().end() &&
            std::any_of(owner->second.settings.begin(),owner->second.settings.end(),
                [](const Setting& channel) {
                    return (channel.id=="accent_red" || channel.id=="accent_green" ||
                        channel.id=="accent_blue") && !IsDefault(channel);
                });
        if (accentChanged) {
            ImGui::SameLine();
            if (ResetButton("Restore the default accent color"))
                for (const auto& channel:owner->second.settings)
                    if (channel.id=="accent_red" || channel.id=="accent_green" || channel.id=="accent_blue")
                        Registry::Instance().Queue(mod,channel.id,channel.initial);
        }
        ImGui::PopID();
        return;
    }
    ImGui::PushID(s.id.c_str());
    ScValue v=s.value; bool changed=false;
    if (s.type==SC_HEADER) ImGui::SeparatorText(s.label.c_str());
    else if (s.type==SC_DIVIDER) ImGui::Separator();
    else if (s.type==SC_DESCRIPTION) ImGui::TextWrapped("%s",s.description.c_str());
    else if (s.type==SC_INFO) {
        ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_SliderGrab],"%s",s.label.c_str());
        ImGui::TextWrapped("%s",v.text);
    }
    else {
        ImGui::BeginGroup();
        if (s.flags&SC_READ_ONLY) ImGui::BeginDisabled();
        const float controlWidth=std::max(1.0f,ImGui::GetContentRegionAvail().x-
            ImGui::GetFrameHeight()-ImGui::GetStyle().ItemSpacing.x);
        ImGui::BeginGroup();
        switch (s.type) {
        case SC_BOOL: {bool b=v.integer!=0;changed=ImGui::Checkbox(s.label.c_str(),&b);
            v.integer=b?1:0;break;}
        case SC_INT: {ImGui::TextUnformatted(s.label.c_str());
            int n=v.integer;ImGui::SetNextItemWidth(controlWidth);
            const int minimum=static_cast<int>(s.minimum),maximum=static_cast<int>(s.maximum);
            changed=ExactSlider("##value",ImGuiDataType_S32,&n,&minimum,&maximum,"%d");
            if (changed) n=static_cast<int>(SnapValue(n,s));
            v.integer=n;break;}
        case SC_FLOAT: {ImGui::TextUnformatted(s.label.c_str());
            float f=v.real;ImGui::SetNextItemWidth(controlWidth);
            const float minimum=static_cast<float>(s.minimum),maximum=static_cast<float>(s.maximum);
            changed=ExactSlider("##value",ImGuiDataType_Float,&f,&minimum,&maximum,"%.6g");
            if (changed && std::isfinite(f)) {
                f=static_cast<float>(SnapValue(f,s));
                if (f<s.minimum) f=std::nextafter(f,maximum);
                if (f>s.maximum) f=std::nextafter(f,minimum);
            }
            v.real=f;break;}
        case SC_COLOR: {
            float color[3]{
                static_cast<float>((v.integer>>16)&0xFF)/255.0f,
                static_cast<float>((v.integer>>8)&0xFF)/255.0f,
                static_cast<float>(v.integer&0xFF)/255.0f};
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(s.label.c_str());ImGui::SameLine();
            if (ImGui::ColorButton("##preview",ImVec4(color[0],color[1],color[2],1),
                    ImGuiColorEditFlags_NoTooltip,ImVec2(48,22)))
                ImGui::OpenPopup("picker");
            if (ImGui::BeginPopup("picker")) {
                if (ImGui::ColorPicker3("##picker",color,
                        ImGuiColorEditFlags_PickerHueWheel|
                        ImGuiColorEditFlags_NoSidePreview)) {
                    auto channel=[](float f) {return static_cast<int>(
                        std::lround(std::clamp(f,0.0f,1.0f)*255.0f));};
                    v.integer=(channel(color[0])<<16)|
                        (channel(color[1])<<8)|channel(color[2]);
                    changed=true;
                }
                ImGui::EndPopup();
            }
            break;}
        case SC_ENUM: {
            ImGui::TextUnformatted(s.label.c_str());ImGui::SetNextItemWidth(controlWidth);
            std::vector<std::string> labels; size_t start=0;
            do {auto end=s.options.find('|',start);labels.push_back(s.options.substr(start,end-start));
                if (end==std::string::npos) break;start=end+1;} while (true);
            const int index=v.integer-static_cast<int>(s.minimum);
            const char* preview=index>=0&&index<static_cast<int>(labels.size())?
                labels[index].c_str():"?";
            if (ImGui::BeginCombo("##value",preview)) {
                for (size_t i=0;i<labels.size();++i)
                    if (ImGui::Selectable(labels[i].c_str(),index==static_cast<int>(i))) {
                        v.integer=static_cast<int>(s.minimum)+static_cast<int>(i);changed=true;
                    }
                ImGui::EndCombo();
            }break;}
        case SC_STRING: {ImGui::TextUnformatted(s.label.c_str());
            char buf[SC_TEXT_CAP]{};std::memcpy(buf,v.text,sizeof buf);
            ImGui::SetNextItemWidth(controlWidth);
            changed=ImGui::InputText("##value",buf,sizeof buf);
            if (changed) std::memcpy(v.text,buf,sizeof buf);break;}
        case SC_KEY: {
            char keyName[64]{};
            const UINT scan=MapVirtualKeyA(static_cast<UINT>(v.integer),MAPVK_VK_TO_VSC);
            if (!scan || !GetKeyNameTextA(static_cast<LONG>(scan<<16),keyName,sizeof keyName))
                std::snprintf(keyName,sizeof keyName,"VK %d",v.integer);
            char label[160];std::snprintf(label,sizeof label,"%s: %s",s.label.c_str(),keyName);
            if (ImGui::Button(label)) {keyCaptureMod_=mod;keyCaptureId_=s.id;
                notice_="Press a key (Escape cancels)";}
            break;}
        case SC_BUTTON: if (ImGui::Button(s.label.c_str())) changed=true;break;
        default: break;
        }
        ImGui::EndGroup();
        const bool resettable=s.type==SC_BOOL || s.type==SC_INT || s.type==SC_FLOAT ||
            s.type==SC_ENUM || s.type==SC_STRING || s.type==SC_KEY || s.type==SC_COLOR;
        if (resettable && !IsDefault(s)) {
            const float resetY=ImGui::GetItemRectMax().y-ImGui::GetFrameHeight();
            ImGui::SameLine();
            const auto cursor=ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(cursor.x,resetY));
            if (ResetButton()) {
                v=s.initial;changed=true;
                if (keyCaptureMod_==mod && keyCaptureId_==s.id) {
                    keyCaptureMod_=0;keyCaptureId_.clear();
                }
            }
        }
        if (s.flags&SC_READ_ONLY) ImGui::EndDisabled();
        if (s.flags&SC_RELOAD_MOD)
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_SliderGrab],"Applies after mod reload");
        if (s.flags&SC_RESTART_GAME)
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_SliderGrab],"Requires game restart");
        if (!s.description.empty()) ImGui::TextWrapped("%s",s.description.c_str());
        ImGui::EndGroup();
        if (ImGui::IsItemHovered()&&!s.tooltip.empty()) ImGui::SetTooltip("%s",s.tooltip.c_str());
        if (changed) {
            const auto result=Registry::Instance().Set(mod,s.id.c_str(),&v);
            if (result!=SC_OK) notice_="Value does not match this option's limits or step";
        }
    }
    ImGui::PopID();
}
}
