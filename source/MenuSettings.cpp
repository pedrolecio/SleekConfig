#define IMGUI_DEFINE_MATH_OPERATORS
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
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
    const bool clicked=ImGui::Button("##reset-default",ImVec2(size,size));
    ImGui::PopStyleColor();
    const auto corner=ImGui::GetItemRectMin();
    const ImVec2 center(corner.x+size*0.5f,corner.y+size*0.5f);
    const float radius=size*0.23f;
    const ImU32 color=ImGui::GetColorU32(ImGui::IsItemHovered()?ImGuiCol_Text:ImGuiCol_SliderGrab);
    auto* draw=ImGui::GetWindowDrawList();
    draw->PathArcTo(center,radius,-2.4f,2.4f,20);
    draw->PathStroke(color,0,1.6f);
    const ImVec2 tip(center.x+radius*std::cos(2.4f),center.y+radius*std::sin(2.4f));
    const ImVec2 base(tip.x+std::sin(2.4f)*size*0.15f,
        tip.y-std::cos(2.4f)*size*0.15f);
    const ImVec2 wing(std::cos(2.4f)*size*0.09f,std::sin(2.4f)*size*0.09f);
    draw->AddTriangleFilled(tip,ImVec2(base.x+wing.x,base.y+wing.y),
        ImVec2(base.x-wing.x,base.y-wing.y),color);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",tooltip);
    return clicked;
}
/* Reserves the reset column so every control keeps the same width. */
void ResetPlaceholder() {
    const float size=ImGui::GetFrameHeight();
    ImGui::Dummy(ImVec2(size,size));
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
int DecimalsFor(double quantum) {
    if (!(quantum>0)) return 3;
    for (int d=0;d<6;++d) {
        const double scaled=quantum*std::pow(10.0,d);
        if (std::abs(scaled-std::round(scaled))<1e-6*std::max(1.0,scaled)) return d;
    }
    return 6;
}
void SetTooltipIfHovered(const char* text) {
    if (text && *text && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        ImGui::SetTooltip("%s",text);
}
}
SliderScale ScaleFor(const Setting& s) {
    SliderScale scale{};
    const bool integer=s.type!=SC_FLOAT;
    const double range=std::max(0.0,s.maximum-s.minimum);
    if (s.step>0) {
        scale.step=s.step;scale.fine=s.step;scale.coarse=s.step*10;
    } else {
        // One hundredth of the range's decade: 0..1 moves 0.01, -2000..2000 moves 10.
        const double unit=range>0?std::pow(10.0,std::floor(std::log10(range)))/100.0:0.01;
        scale.step=unit;scale.fine=unit/10;scale.coarse=unit*10;
    }
    if (integer) {
        scale.step=std::max(1.0,std::round(scale.step));
        scale.fine=std::max(1.0,std::round(scale.fine));
        scale.coarse=std::max(scale.step,std::round(scale.coarse));
        scale.decimals=0;
    } else scale.decimals=DecimalsFor(scale.fine);
    return scale;
}
double QuantizeSlider(double value,const Setting& s,const SliderScale& scale) {
    if (s.step<=0 && scale.fine>0) value=std::round(value/scale.fine)*scale.fine;
    return SnapValue(value,s);
}
bool Menu::FineSlider(const Setting& s,double& value) {
    ImGuiWindow* window=ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g=*GImGui;
    const ImGuiStyle& style=g.Style;
    const ImGuiID id=window->GetID("##value");
    const bool integer=s.type!=SC_FLOAT;
    const SliderScale scale=ScaleFor(s);
    const double range=s.maximum-s.minimum;
    const float width=ImGui::CalcItemWidth();
    const ImRect frame(window->DC.CursorPos,
        window->DC.CursorPos+ImVec2(width,ImGui::GetFrameHeight()));
    ImGui::ItemSize(frame,style.FramePadding.y);
#ifdef SC_TEST
    sliderRect_[0]=frame.Min.x;sliderRect_[1]=frame.Min.y;
    sliderRect_[2]=frame.Max.x;sliderRect_[3]=frame.Max.y;
#endif
    if (!ImGui::ItemAdd(frame,id,&frame,ImGuiItemFlags_Inputable)) return false;
    const bool disabled=(g.LastItemData.ItemFlags&ImGuiItemFlags_Disabled)!=0;
    const bool hovered=ImGui::ItemHoverable(frame,id,g.LastItemData.ItemFlags);
    int decimals=scale.decimals;
    if (!integer) while (decimals<6) {
        const double scaled=value*std::pow(10.0,decimals);
        if (std::abs(scaled-std::round(scaled))<=0.01) break;
        ++decimals;
    }
    char format[16];
    if (integer) std::snprintf(format,sizeof format,"%%d");
    else std::snprintf(format,sizeof format,"%%.%df",decimals);

    bool changed=false;
    bool temp=!disabled && ImGui::TempInputIsActive(id);
    if (!temp && !disabled) {
        const bool clicked=hovered && ImGui::IsMouseClicked(0,ImGuiInputFlags_None,id);
        if (clicked) ImGui::SetKeyOwner(ImGuiKey_MouseLeft,id);
        if (clicked && (g.IO.KeyCtrl || ImGui::IsMouseDoubleClicked(0))) temp=true;
        else if (clicked) {
            ImGui::SetActiveID(id,window);ImGui::SetFocusID(id,window);ImGui::FocusWindow(window);
            sliderDrag_={id,value,g.IO.MousePos.x,g.IO.KeyShift,false};
            const float grabWidth=std::max(style.GrabMinSize,10.0f);
            const float usable=std::max(1.0f,frame.GetWidth()-grabWidth);
            const float t=range>0?static_cast<float>((value-s.minimum)/range):0.0f;
            const float grabX=frame.Min.x+grabWidth*0.5f+usable*std::clamp(t,0.0f,1.0f);
            // Pressing the grab (or holding Shift) keeps the value; pressing
            // the track jumps there and keeps dragging from that point.
            if (!g.IO.KeyShift && std::abs(g.IO.MousePos.x-grabX)>grabWidth*0.5f+2.0f &&
                range>0) {
                const double jumped=QuantizeSlider(s.minimum+range*std::clamp(
                    (g.IO.MousePos.x-frame.Min.x-grabWidth*0.5f)/usable,0.0f,1.0f),s,scale);
                sliderDrag_.startValue=jumped;sliderDrag_.moved=true;
                if (jumped!=value) {value=jumped;changed=true;}
            }
        }
    }
    if (temp) {
        bool edited=false;
        if (integer) {
            int number=static_cast<int>(std::lround(value));
            const int minimum=static_cast<int>(s.minimum),maximum=static_cast<int>(s.maximum);
            edited=ImGui::TempInputScalar(frame,id,"##value",ImGuiDataType_S32,&number,"%d",
                &minimum,&maximum);
            if (edited) value=SnapValue(number,s);
        } else {
            float real=static_cast<float>(value);
            const float minimum=static_cast<float>(s.minimum),maximum=static_cast<float>(s.maximum);
            edited=ImGui::TempInputScalar(frame,id,"##value",ImGuiDataType_Float,&real,format,
                &minimum,&maximum);
            if (edited && std::isfinite(real)) value=SnapValue(real,s);
            else edited=false;
        }
        return edited;
    }
    if (g.ActiveId==id && sliderDrag_.id==id) {
        if (!g.IO.MouseDown[0]) ImGui::ClearActiveID();
        else if (range>0) {
            const bool fine=g.IO.KeyShift;
            if (fine!=sliderDrag_.fine) {
                // Changing speed mid-drag continues from the current value.
                sliderDrag_.fine=fine;sliderDrag_.startValue=value;
                sliderDrag_.startX=g.IO.MousePos.x;
            }
            // A press on the grab that jitters by a pixel or two changes nothing.
            const float dx=g.IO.MousePos.x-sliderDrag_.startX;
            if (!sliderDrag_.moved && std::abs(dx)>3.0f) sliderDrag_.moved=true;
            if (sliderDrag_.moved) {
                const float grabWidth=std::max(style.GrabMinSize,10.0f);
                const float usable=std::max(1.0f,frame.GetWidth()-grabWidth);
                const double speed=fine?fineDragSpeed_:dragSpeed_;
                const double next=QuantizeSlider(sliderDrag_.startValue+
                    dx/usable*range*speed,s,scale);
                if (next!=value) {value=next;changed=true;}
            }
        }
    }
    // After a click the slider keeps keyboard focus: arrows and the wheel nudge it.
    const bool focused=!disabled && g.NavId==id && g.NavWindow==window;
    if (focused && (g.ActiveId==0 || g.ActiveId==id) && !g.IO.WantTextInput) {
        const double unit=g.IO.KeyShift?scale.fine:g.IO.KeyCtrl?scale.coarse:scale.step;
        int direction=0;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow,true)) ++direction;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,true)) --direction;
        if (hovered) {
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,id);
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelX,id);
            const float wheel=g.IO.MouseWheel+g.IO.MouseWheelH;
            if (ImGui::GetKeyOwner(ImGuiKey_MouseWheelY)==id && wheel!=0)
                direction+=wheel>0?1:-1;
        }
        if (direction) {
            const double next=QuantizeSlider(value+direction*unit,s,scale);
            if (next!=value) {value=next;changed=true;}
        }
    }

    const bool active=g.ActiveId==id;
    auto* draw=window->DrawList;
    const ImU32 frameColor=ImGui::GetColorU32(active?ImGuiCol_FrameBgActive:
        hovered?ImGuiCol_FrameBgHovered:ImGuiCol_FrameBg);
    ImGui::RenderFrame(frame.Min,frame.Max,frameColor,false,style.FrameRounding);
    const float grabWidth=std::max(style.GrabMinSize,10.0f);
    const float usable=std::max(1.0f,frame.GetWidth()-grabWidth);
    const auto position=[&](double v) {
        const float t=range>0?static_cast<float>((v-s.minimum)/range):0.0f;
        return frame.Min.x+grabWidth*0.5f+usable*std::clamp(t,0.0f,1.0f);
    };
    const float grabX=position(value);
    // Fill from zero when the range crosses it, otherwise from the minimum.
    const float origin=s.minimum<0 && s.maximum>0?position(0.0):frame.Min.x;
    ImVec4 fill=style.Colors[ImGuiCol_SliderGrab];fill.w*=disabled?0.15f:0.30f;
    if (std::abs(grabX-origin)>0.5f)
        draw->AddRectFilled(ImVec2(std::min(origin,grabX),frame.Min.y+1),
            ImVec2(std::max(origin,grabX),frame.Max.y-1),ImGui::GetColorU32(fill),
            style.FrameRounding);
    const float inset=std::max(2.0f,frame.GetHeight()*0.16f);
    draw->AddRectFilled(ImVec2(grabX-grabWidth*0.5f+2,frame.Min.y+inset),
        ImVec2(grabX+grabWidth*0.5f-2,frame.Max.y-inset),
        ImGui::GetColorU32(active?ImGuiCol_SliderGrabActive:ImGuiCol_SliderGrab),
        style.GrabRounding);
    if (focused)
        draw->AddRect(frame.Min,frame.Max,ImGui::GetColorU32(ImGuiCol_SliderGrab,0.75f),
            style.FrameRounding,0,1.0f);
    char text[64];
    if (integer) std::snprintf(text,sizeof text,format,static_cast<int>(std::lround(value)));
    else std::snprintf(text,sizeof text,format,value);
    ImGui::RenderTextClipped(frame.Min,frame.Max,text,nullptr,nullptr,ImVec2(0.5f,0.5f));
    if (hovered && !active && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        ImGui::SetTooltip("Drag to adjust; hold Shift for fine control\n"
            "Ctrl+click or double-click to type a value\n"
            "After clicking: Left/Right or mouse wheel step %g (Shift %g, Ctrl %g)",
            scale.step,scale.fine,scale.coarse);
    if (changed) ImGui::MarkItemEdited(id);
    return changed;
}
bool ToggleSwitch(const char* id,bool* value) {
    const float height=ImGui::GetFrameHeight()*0.78f;
    const float width=height*1.85f;
    const ImVec2 corner=ImGui::GetCursorScreenPos();
    const float offsetY=(ImGui::GetFrameHeight()-height)*0.5f;
    const bool clicked=ImGui::InvisibleButton(id,ImVec2(width,ImGui::GetFrameHeight()));
    if (clicked) *value=!*value;
    const bool hovered=ImGui::IsItemHovered();
    auto* draw=ImGui::GetWindowDrawList();
    const auto& colors=ImGui::GetStyle().Colors;
    ImVec4 track=*value?colors[ImGuiCol_SliderGrab]:colors[ImGuiCol_FrameBg];
    if (hovered) track=*value?colors[ImGuiCol_SliderGrabActive]:colors[ImGuiCol_FrameBgHovered];
    if (ImGui::GetItemFlags()&ImGuiItemFlags_Disabled) track.w*=0.5f;
    const ImVec2 min(corner.x,corner.y+offsetY),max(corner.x+width,corner.y+offsetY+height);
    draw->AddRectFilled(min,max,ImGui::GetColorU32(track),height*0.5f);
    const float radius=height*0.5f-2.5f;
    const float x=*value?max.x-height*0.5f:min.x+height*0.5f;
    draw->AddCircleFilled(ImVec2(x,min.y+height*0.5f),radius,
        ImGui::GetColorU32(*value?ImVec4(1,1,1,1):colors[ImGuiCol_TextDisabled]));
    return clicked;
}
void Menu::DrawRowLabel(const Setting& s,float width,bool clickable,bool* toggled) {
    const ImVec2 corner=ImGui::GetCursorScreenPos();
    const float height=ImGui::GetFrameHeight();
    if (clickable) {
        if (ImGui::InvisibleButton("##label",ImVec2(width,height)) && toggled) *toggled=true;
    } else ImGui::Dummy(ImVec2(width,height));
    const bool hovered=ImGui::IsItemHovered();
    const auto& style=ImGui::GetStyle();
    float textMax=corner.x+width-style.ItemSpacing.x;
    const bool flagged=(s.flags&(SC_RELOAD_MOD|SC_RESTART_GAME))!=0;
    if (flagged) textMax-=10;
    const ImVec2 textMin(corner.x,corner.y+style.FramePadding.y);
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(),textMin,
        ImVec2(textMax,corner.y+height),textMax,s.label.c_str(),nullptr,nullptr);
    if (flagged) {
        const float textWidth=std::min(ImGui::CalcTextSize(s.label.c_str()).x,textMax-corner.x);
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(corner.x+textWidth+7,corner.y+height*0.5f),2.6f,
            ImGui::GetColorU32((s.flags&SC_RESTART_GAME)?ImVec4(1.0f,0.66f,0.30f,1.0f):
                style.Colors[ImGuiCol_SliderGrab]));
    }
    if (hovered && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize()*26);
        ImGui::TextUnformatted(s.label.c_str());
        if (!s.description.empty()) ImGui::TextDisabled("%s",s.description.c_str());
        if (!s.tooltip.empty()) ImGui::TextDisabled("%s",s.tooltip.c_str());
        if (s.flags&SC_RELOAD_MOD) ImGui::TextColored(style.Colors[ImGuiCol_SliderGrab],
            "Applies after mod reload");
        if (s.flags&SC_RESTART_GAME) ImGui::TextColored(ImVec4(1.0f,0.66f,0.30f,1.0f),
            "Requires game restart");
        if (s.type==SC_INT || s.type==SC_FLOAT)
            ImGui::TextDisabled("Range %g to %g  |  id %s",s.minimum,s.maximum,s.id.c_str());
        else ImGui::TextDisabled("id %s",s.id.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::SameLine(0,0);
}
void Menu::DrawSetting(ScMod mod,const Setting& s) {
    if (mod==ownMod_ && (s.id=="accent_green" || s.id=="accent_blue"))
        return;
    const auto& style=ImGui::GetStyle();
    const float avail=std::max(1.0f,ImGui::GetContentRegionAvail().x);
    const float labelWidth=std::clamp(avail*0.42f,110.0f,330.0f);
    const float resetWidth=ImGui::GetFrameHeight();
    const float controlWidth=std::max(40.0f,avail-labelWidth-resetWidth-style.ItemSpacing.x);
    if (mod==ownMod_ && s.id=="accent_red") {
        ImGui::PushID("theme-accent");
        const ImVec2 rowMin=ImGui::GetCursorScreenPos();
        ImGui::BeginGroup();
        Setting row=s;row.label="Accent color";
        row.description="Color used for highlights, sliders and selection.";
        DrawRowLabel(row,labelWidth,false,nullptr);
        if (ImGui::ColorButton("##preview",
                ImVec4(accent_[0],accent_[1],accent_[2],1.0f),
                ImGuiColorEditFlags_NoTooltip,ImVec2(std::min(controlWidth,140.0f),
                    ImGui::GetFrameHeight()))) {
            for (int i=0;i<3;++i) pickerColor_[i]=accent_[i];
            ImGui::OpenPopup("picker");
        }
        SetTooltipIfHovered("Click to choose the accent color");
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
        const auto owner=Registry::Instance().Mods().find(mod);
        const bool accentChanged=owner!=Registry::Instance().Mods().end() &&
            std::any_of(owner->second.settings.begin(),owner->second.settings.end(),
                [](const Setting& channel) {
                    return (channel.id=="accent_red" || channel.id=="accent_green" ||
                        channel.id=="accent_blue") && !IsDefault(channel);
                });
        ImGui::SameLine(0,0);
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x+avail-resetWidth,rowMin.y));
        if (accentChanged) {
            if (ResetButton("Restore the default accent color"))
                for (const auto& channel:owner->second.settings)
                    if (channel.id=="accent_red" || channel.id=="accent_green" || channel.id=="accent_blue")
                        Registry::Instance().Queue(mod,channel.id,channel.initial);
        } else ResetPlaceholder();
        ImGui::EndGroup();
        ImGui::PopID();
        return;
    }
    ImGui::PushID(s.id.c_str());
    ScValue v=s.value; bool changed=false;
    if (s.type==SC_HEADER) {
        ImGui::PushStyleColor(ImGuiCol_Text,style.Colors[ImGuiCol_SliderGrabActive]);
        ImGui::SeparatorText(s.label.c_str());
        ImGui::PopStyleColor();
    }
    else if (s.type==SC_DIVIDER) ImGui::Separator();
    else if (s.type==SC_DESCRIPTION) {
        ImGui::PushStyleColor(ImGuiCol_Text,style.Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("%s",s.description.c_str());
        ImGui::PopStyleColor();
    }
    else if (s.type==SC_INFO) {
        ImGui::BeginGroup();
        DrawRowLabel(s,labelWidth,false,nullptr);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+controlWidth+resetWidth);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled],"%s",v.text);
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
    }
    else {
        auto* draw=ImGui::GetWindowDrawList();
        draw->ChannelsSplit(2);draw->ChannelsSetCurrent(1);
        const ImVec2 rowMin=ImGui::GetCursorScreenPos();
        ImGui::BeginGroup();
        if (s.flags&SC_READ_ONLY) ImGui::BeginDisabled();
        bool toggledByLabel=false;
        DrawRowLabel(s,labelWidth,s.type==SC_BOOL && !(s.flags&SC_READ_ONLY),&toggledByLabel);
        ImGui::BeginGroup();
        switch (s.type) {
        case SC_BOOL: {
            bool b=v.integer!=0;
            changed=ToggleSwitch("##value",&b);
            if (toggledByLabel) {b=!b;changed=true;}
            v.integer=b?1:0;
            const float used=ImGui::GetItemRectSize().x;
            if (controlWidth>used) {ImGui::SameLine(0,0);ImGui::Dummy(ImVec2(controlWidth-used,1));}
            break;}
        case SC_INT: case SC_FLOAT: {
            double number=s.type==SC_INT?static_cast<double>(v.integer):static_cast<double>(v.real);
            ImGui::SetNextItemWidth(controlWidth);
            changed=FineSlider(s,number);
            if (s.type==SC_INT) v.integer=static_cast<int>(std::lround(number));
            else {
                float f=static_cast<float>(number);
                if (f<s.minimum) f=std::nextafter(f,static_cast<float>(s.maximum));
                if (f>s.maximum) f=std::nextafter(f,static_cast<float>(s.minimum));
                v.real=f;
            }
            break;}
        case SC_COLOR: {
            float color[3]{
                static_cast<float>((v.integer>>16)&0xFF)/255.0f,
                static_cast<float>((v.integer>>8)&0xFF)/255.0f,
                static_cast<float>(v.integer&0xFF)/255.0f};
            const float swatch=std::min(controlWidth,140.0f);
            if (ImGui::ColorButton("##preview",ImVec4(color[0],color[1],color[2],1),
                    ImGuiColorEditFlags_NoTooltip,ImVec2(swatch,ImGui::GetFrameHeight())))
                ImGui::OpenPopup("picker");
            SetTooltipIfHovered("Click to choose a color");
            if (controlWidth>swatch+40) {
                ImGui::SameLine();ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("#%06X",static_cast<unsigned>(v.integer)&0xFFFFFFu);
            }
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
            ImGui::SetNextItemWidth(controlWidth);
            std::vector<std::string> labels; size_t start=0;
            do {auto end=s.options.find('|',start);labels.push_back(s.options.substr(start,end-start));
                if (end==std::string::npos) break;start=end+1;} while (true);
            const int index=v.integer-static_cast<int>(s.minimum);
            const char* preview=index>=0&&index<static_cast<int>(labels.size())?
                labels[index].c_str():"?";
            if (ImGui::BeginCombo("##value",preview,ImGuiComboFlags_HeightLarge)) {
                for (size_t i=0;i<labels.size();++i)
                    if (ImGui::Selectable(labels[i].c_str(),index==static_cast<int>(i))) {
                        v.integer=static_cast<int>(s.minimum)+static_cast<int>(i);changed=true;
                    }
                ImGui::EndCombo();
            }break;}
        case SC_STRING: {
            char buf[SC_TEXT_CAP]{};std::memcpy(buf,v.text,sizeof buf);
            ImGui::SetNextItemWidth(controlWidth);
            changed=ImGui::InputText("##value",buf,sizeof buf);
            if (changed) std::memcpy(v.text,buf,sizeof buf);break;}
        case SC_KEY: {
            char keyName[64]{};
            const UINT scan=MapVirtualKeyA(static_cast<UINT>(v.integer),MAPVK_VK_TO_VSC);
            if (!scan || !GetKeyNameTextA(static_cast<LONG>(scan<<16),keyName,sizeof keyName))
                std::snprintf(keyName,sizeof keyName,"VK %d",v.integer);
            const bool capturing=keyCaptureMod_==mod && keyCaptureId_==s.id;
            if (capturing) ImGui::PushStyleColor(ImGuiCol_Button,style.Colors[ImGuiCol_HeaderActive]);
            char label[96];
            std::snprintf(label,sizeof label,"%s##key",capturing?"Press a key...":keyName);
            if (ImGui::Button(label,ImVec2(std::min(controlWidth,180.0f),0))) {
                keyCaptureMod_=mod;keyCaptureId_=s.id;
                notice_="Press a key (Escape cancels)";
            }
            if (capturing) ImGui::PopStyleColor();
            SetTooltipIfHovered("Click, then press the new key");
            break;}
        case SC_BUTTON: {
            if (ImGui::Button(s.label.c_str(),ImVec2(std::min(controlWidth,
                    std::max(120.0f,ImGui::CalcTextSize(s.label.c_str()).x+
                        style.FramePadding.x*4)),0))) changed=true;
            break;}
        default: break;
        }
        ImGui::EndGroup();
        // Keep the reset column aligned regardless of the control's own width.
        ImGui::SameLine(0,0);
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x+avail-resetWidth,rowMin.y));
        const bool resettable=s.type==SC_BOOL || s.type==SC_INT || s.type==SC_FLOAT ||
            s.type==SC_ENUM || s.type==SC_STRING || s.type==SC_KEY || s.type==SC_COLOR;
        if (resettable && !IsDefault(s)) {
            if (ResetButton()) {
                v=s.initial;changed=true;
                if (keyCaptureMod_==mod && keyCaptureId_==s.id) {
                    keyCaptureMod_=0;keyCaptureId_.clear();
                }
            }
        } else ResetPlaceholder();
        if (s.flags&SC_READ_ONLY) ImGui::EndDisabled();
        if (inlineDescriptions_ && !s.description.empty()) {
            ImGui::PushFont(nullptr,std::max(11.0f,ImGui::GetFontSize()-2.5f));
            ImGui::PushStyleColor(ImGuiCol_Text,style.Colors[ImGuiCol_TextDisabled]);
            ImGui::PushTextWrapPos(rowMin.x-ImGui::GetWindowPos().x+avail-resetWidth);
            ImGui::TextWrapped("%s",s.description.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();ImGui::PopFont();
        }
        ImGui::EndGroup();
        const ImVec2 rowMax(rowMin.x+avail,ImGui::GetItemRectMax().y);
        draw->ChannelsSetCurrent(0);
        if (ImGui::IsMouseHoveringRect(rowMin,rowMax) && ImGui::IsWindowHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem|ImGuiHoveredFlags_ChildWindows))
            draw->AddRectFilled(ImVec2(rowMin.x-4,rowMin.y-1),ImVec2(rowMax.x+2,rowMax.y+1),
                ImGui::GetColorU32(ImGuiCol_FrameBg,0.45f),style.FrameRounding);
        draw->ChannelsMerge();
        if (changed) {
            const auto result=Registry::Instance().Set(mod,s.id.c_str(),&v);
            if (result!=SC_OK) notice_="Value does not match this option's limits or step";
        }
    }
    ImGui::PopID();
}
}
