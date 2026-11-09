#include "Menu.h"
#include "Registry.h"
#include "Persistence.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cmath>
#include <cstring>

namespace sc {
struct MenuTestAccess {
    static void Draw(Menu& menu,ScMod mod,const Setting& setting) {menu.DrawSetting(mod,setting);}
    static void Own(Menu& menu,ScMod mod) {menu.ownMod_=mod;}
    static void Capture(Menu& menu,ScMod mod,const char* id) {
        menu.keyCaptureMod_=mod;menu.keyCaptureId_=id;
    }
    static bool Capturing(const Menu& menu) {return menu.keyCaptureMod_!=0;}
    static ImRect Slider(const Menu& menu) {
        return ImRect(menu.sliderRect_[0],menu.sliderRect_[1],menu.sliderRect_[2],menu.sliderRect_[3]);
    }
    static void Speeds(Menu& menu,float normal,float fine) {
        menu.dragSpeed_=normal;menu.fineDragSpeed_=fine;
    }
};
}
namespace {
sc::Menu menu;
ScMod mod{};
const char* currentSetting="level";
ImRect bounds;
ImGuiID sliderId{};
int changes{};
bool reject{};
int SC_CALL Changed(void*,const char*,const ScValue*) {++changes;return !reject;}
const sc::Setting& Setting() {
    for (const auto& s:sc::Registry::Instance().Mods().at(mod).settings)
        if (s.id==currentSetting) return s;
    assert(false);return sc::Registry::Instance().Mods().at(mod).settings.front();
}
void Frame(float delta=1.0f/60) {
    auto& io=ImGui::GetIO();io.DeltaTime=delta;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(600,360));
    ImGui::Begin("Settings test",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoResize);
    ImGui::PushID(static_cast<int>(mod));
    ImGui::PushID(currentSetting);sliderId=ImGui::GetID("##value");ImGui::PopID();
    sc::MenuTestAccess::Draw(menu,mod,Setting());
    bounds=ImGui::GetCurrentContext()->LastItemData.Rect;
    ImGui::PopID();ImGui::End();ImGui::Render();
    sc::Registry::Instance().Tick();
}
ImVec2 ResetPosition() {
    const float size=ImGui::GetFrameHeight();
    assert(bounds.Max.x<=600 && bounds.Max.y<=360);
    return ImVec2(bounds.Max.x-size/2,bounds.Max.y-size/2);
}
// Rows are label | control | reset; aim inside the control, left of the reset column.
ImVec2 SliderPosition() {
    const float size=ImGui::GetFrameHeight();
    return ImVec2(bounds.Max.x-size-ImGui::GetStyle().ItemSpacing.x-40,bounds.Max.y-size/2);
}
void Click(ImVec2 position) {
    auto& io=ImGui::GetIO();io.AddMousePosEvent(position.x,position.y);Frame(0.6f);
    io.AddMouseButtonEvent(0,true);Frame();io.AddMouseButtonEvent(0,false);Frame();
}
void ClickReset() {
    const auto position=ResetPosition();
    Click(position);
    const auto afterReset=changes;
    // After the committed value returns to its default, the old icon area
    // must have no button and cannot produce another change callback.
    Click(position);assert(changes==afterReset);
}
void Type(const char* text) {
    auto& io=ImGui::GetIO();io.AddInputCharactersUTF8(text);Frame();
    io.AddKeyEvent(ImGuiKey_Enter,true);Frame();io.AddKeyEvent(ImGuiKey_Enter,false);Frame();
}
void CtrlInput(const char* text) {
    Frame();const auto position=SliderPosition();auto& io=ImGui::GetIO();
    io.AddMousePosEvent(position.x,position.y);io.AddKeyEvent(ImGuiMod_Ctrl,true);Frame(0.6f);
    io.AddMouseButtonEvent(0,true);Frame();
    assert(ImGui::TempInputIsActive(sliderId));
    io.AddMouseButtonEvent(0,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);Frame();Type(text);
}
ScSettingDesc Integer(const char* id,uint32_t type=SC_INT,int initial=5) {
    ScSettingDesc s{};s.size=sizeof(s);s.id=id;s.label=id;s.type=type;
    s.minimum=0;s.maximum=100;s.step=1;s.default_value.type=type;s.default_value.integer=initial;
    return s;
}
void Set(const char* id,ScValue value) {
    assert(sc::Registry::Instance().Set(mod,id,&value)==SC_OK);sc::Registry::Instance().Tick();
}
}
int main() {
    auto& r=sc::Registry::Instance();
    DeleteFileA(sc::DataPath("test.menu").c_str());
    ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize=ImVec2(640,480);
    unsigned char* pixels{};int width{},height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ScModDesc d{};d.size=sizeof(d);d.required_api=1;d.id="test.menu";d.name="Menu";
    d.on_change=Changed;assert(r.Register(&d,&mod)==SC_OK);
    auto level=Integer("level");int32_t number{};
    ScBindingDesc binding{sizeof(binding),SC_STORAGE_INT32,&number,sizeof(number)};
    assert(r.Add(mod,&level,&binding)==SC_OK);
    auto other=Integer("other",SC_INT,7);assert(r.Add(mod,&other)==SC_OK);
    r.Tick();ScValue v=level.default_value;v.integer=60;Set("level",v);
    v.integer=20;Set("other",v);
    Frame();ClickReset();assert(number==5);
    assert(r.Get(mod,"other",&v)==SC_OK && v.integer==20);
    assert(r.Mods().at(mod).dirty);

    CtrlInput("17");assert(number==17);
    CtrlInput("999");assert(number==100);
    CtrlInput("-25");assert(number==0);

    // The second plain click switches the actual slider into native text input.
    Frame();const auto position=SliderPosition();io.AddMousePosEvent(position.x,position.y);Frame(0.6f);
    io.AddMouseButtonEvent(0,true);Frame();io.AddMouseButtonEvent(0,false);Frame();
    const auto beforeDouble=number;
    io.AddMouseButtonEvent(0,true);Frame();
    assert(ImGui::TempInputIsActive(sliderId) && number==beforeDouble);
    io.AddMouseButtonEvent(0,false);Frame();Type("23");assert(number==23);
    reject=true;Click(ResetPosition());assert(number==23);reject=false;

    auto real=Integer("strength",SC_FLOAT);real.minimum=0;real.maximum=1;real.step=0.05;
    real.default_value={};real.default_value.type=SC_FLOAT;real.default_value.real=0.5f;
    float strength{};binding={sizeof(binding),SC_STORAGE_FLOAT,&strength,sizeof(strength)};
    assert(r.Add(mod,&real,&binding)==SC_OK);currentSetting="strength";
    CtrlInput("0.65");assert(std::abs(strength-0.65f)<0.00001f);
    CtrlInput("0.67");assert(std::abs(strength-0.65f)<0.00001f);
    CtrlInput("2");assert(strength==1.0f);
    Frame();ClickReset();assert(strength==0.5f && number==23);

    auto locked=Integer("locked");locked.flags=SC_READ_ONLY;
    assert(r.Add(mod,&locked)==SC_OK);v=locked.default_value;v.integer=30;Set("locked",v);
    currentSetting="locked";Frame();const auto beforeReadonly=changes;
    Click(ResetPosition());assert(r.Get(mod,"locked",&v)==SC_OK && v.integer==30);
    assert(changes==beforeReadonly);
    const auto lockedPosition=SliderPosition();io.AddMousePosEvent(lockedPosition.x,lockedPosition.y);
    io.AddKeyEvent(ImGuiMod_Ctrl,true);Frame(0.6f);io.AddMouseButtonEvent(0,true);Frame();
    assert(!ImGui::TempInputIsActive(sliderId));io.AddMouseButtonEvent(0,false);
    io.AddKeyEvent(ImGuiMod_Ctrl,false);Frame();

    // All other persisted controls receive the same independent reset action.
    auto toggle=Integer("enabled",SC_BOOL,1);toggle.maximum=1;assert(r.Add(mod,&toggle)==SC_OK);
    v=toggle.default_value;v.integer=0;Set("enabled",v);currentSetting="enabled";
    Frame();ClickReset();assert(r.Get(mod,"enabled",&v)==SC_OK && v.integer==1);
    auto preset=Integer("preset",SC_ENUM,1);preset.maximum=2;preset.options="A|B|C";
    assert(r.Add(mod,&preset)==SC_OK);v=preset.default_value;v.integer=2;Set("preset",v);
    currentSetting="preset";Frame();ClickReset();assert(r.Get(mod,"preset",&v)==SC_OK && v.integer==1);
    auto key=Integer("key",SC_KEY,30);key.maximum=255;assert(r.Add(mod,&key)==SC_OK);
    v=key.default_value;v.integer=40;Set("key",v);currentSetting="key";
    sc::MenuTestAccess::Capture(menu,mod,"key");Frame();ClickReset();
    assert(r.Get(mod,"key",&v)==SC_OK && v.integer==30 && !sc::MenuTestAccess::Capturing(menu));
    auto color=Integer("color",SC_COLOR,0x3399FF);assert(r.Add(mod,&color)==SC_OK);
    v=color.default_value;v.integer=0xABCDEF;Set("color",v);currentSetting="color";
    Frame();ClickReset();assert(r.Get(mod,"color",&v)==SC_OK && v.integer==0x3399FF);
    auto text=Integer("text",SC_STRING);text.default_value={};text.default_value.type=SC_STRING;
    std::memcpy(text.default_value.text,"original",sizeof("original"));assert(r.Add(mod,&text)==SC_OK);
    v=text.default_value;std::memcpy(v.text,"edited",sizeof("edited"));Set("text",v);
    currentSetting="text";Frame();ClickReset();assert(r.Get(mod,"text",&v)==SC_OK && !std::strcmp(v.text,"original"));
    // Text comparison ignores unused bytes after the NUL terminator.
    v.text[SC_TEXT_CAP-1]='x';Set("text",v);
    const auto afterText=changes;ClickReset();assert(changes==afterText);

    // Slider steps follow each range; display precision follows the fine step.
    {
        sc::Setting probe{};probe.type=SC_FLOAT;probe.minimum=0;probe.maximum=1;
        auto scale=sc::ScaleFor(probe);
        assert(std::abs(scale.step-0.01)<1e-12 && std::abs(scale.fine-0.001)<1e-12 &&
            scale.decimals==3);
        probe.minimum=-2000;probe.maximum=2000;scale=sc::ScaleFor(probe);
        assert(scale.step==10 && scale.fine==1 && scale.coarse==100 && scale.decimals==0);
        probe.minimum=0.03;probe.maximum=0.14;scale=sc::ScaleFor(probe);
        assert(std::abs(scale.step-0.001)<1e-12 && scale.decimals==4);
        probe.type=SC_INT;probe.minimum=0;probe.maximum=100;scale=sc::ScaleFor(probe);
        assert(scale.step==1 && scale.fine==1 && scale.coarse==10 && scale.decimals==0);
        probe.type=SC_FLOAT;probe.minimum=0;probe.maximum=1;probe.step=0.25;
        scale=sc::ScaleFor(probe);
        assert(scale.step==0.25 && scale.fine==0.25 && scale.decimals==2);
        assert(sc::QuantizeSlider(0.3,probe,scale)==0.25);
    }
    auto offset=Integer("offset",SC_FLOAT);offset.minimum=-2000;offset.maximum=2000;offset.step=0;
    offset.default_value={};offset.default_value.type=SC_FLOAT;offset.default_value.real=0;
    float offsetValue=-1;
    binding={sizeof(binding),SC_STORAGE_FLOAT,&offsetValue,sizeof(offsetValue)};
    assert(r.Add(mod,&offset,&binding)==SC_OK && offsetValue==0);currentSetting="offset";
    Frame();
    const auto rect=sc::MenuTestAccess::Slider(menu);
    const float grabWidth=std::max(ImGui::GetStyle().GrabMinSize,10.0f);
    const float usable=rect.GetWidth()-grabWidth;
    const ImVec2 grab((rect.Min.x+rect.Max.x)*0.5f,(rect.Min.y+rect.Max.y)*0.5f);
    auto drag=[&](float from,float dx,bool shift) {
        io.AddMousePosEvent(from,grab.y);
        if (shift) io.AddKeyEvent(ImGuiMod_Shift,true);
        Frame(0.6f);io.AddMouseButtonEvent(0,true);Frame();
        io.AddMousePosEvent(from+dx,grab.y);Frame();
        io.AddMouseButtonEvent(0,false);Frame();
        if (shift) io.AddKeyEvent(ImGuiMod_Shift,false);
        Frame();
    };
    auto zero=[&] {v=offset.default_value;v.real=0;Set("offset",v);Frame();assert(offsetValue==0);};
    // Pressing the grab does not move it; the grab then follows the mouse.
    drag(grab.x,2,false);assert(offsetValue==0);
    drag(grab.x,40,false);
    const float normal=offsetValue;
    assert(std::abs(normal-std::round(40/usable*4000))<=1 && normal==std::round(normal));
    zero();
    // Shift drags ten times finer by default.
    drag(grab.x,40,true);
    assert(offsetValue>0 && std::abs(offsetValue-std::round(4/usable*4000))<=1);
    zero();
    // The drag speed setting lowers sensitivity without Shift too.
    sc::MenuTestAccess::Speeds(menu,0.5f,0.1f);
    drag(grab.x,40,false);assert(std::abs(offsetValue-std::round(20/usable*4000))<=1);
    sc::MenuTestAccess::Speeds(menu,1.0f,0.1f);
    zero();
    // Pressing the track jumps there.
    drag(grab.x-usable*0.25f,0,false);assert(std::abs(offsetValue+1000)<=1);
    zero();
    // After a click the slider keeps focus: arrows step 10, Shift 1, Ctrl 100.
    drag(grab.x,0,false);assert(offsetValue==0);
    auto press=[&](ImGuiKey key,ImGuiKey modifier) {
        if (modifier!=ImGuiKey_None) io.AddKeyEvent(modifier,true);
        io.AddKeyEvent(key,true);Frame();io.AddKeyEvent(key,false);Frame();
        if (modifier!=ImGuiKey_None) io.AddKeyEvent(modifier,false);
        Frame();
    };
    press(ImGuiKey_RightArrow,ImGuiKey_None);assert(offsetValue==10);
    press(ImGuiKey_RightArrow,ImGuiMod_Shift);assert(offsetValue==11);
    press(ImGuiKey_LeftArrow,ImGuiMod_Ctrl);assert(offsetValue==-89);
    // The wheel nudges the focused slider under the mouse instead of scrolling.
    io.AddMousePosEvent(grab.x,grab.y);Frame();Frame();
    io.AddMouseWheelEvent(0,1);Frame();Frame();assert(offsetValue==-79);
    io.AddKeyEvent(ImGuiMod_Shift,true);Frame();
    io.AddMouseWheelEvent(0,-1);Frame();Frame();assert(offsetValue==-80);
    io.AddKeyEvent(ImGuiMod_Shift,false);Frame();

    // The menu's single accent color resets its three channels, not background.
    for (const auto id:{"accent_red","accent_green","accent_blue","background"}) {
        real.id=id;assert(r.Add(mod,&real)==SC_OK);
        v=real.default_value;v.real=0.8f;Set(id,v);
    }
    sc::MenuTestAccess::Own(menu,mod);currentSetting="accent_red";
    Frame();ClickReset();
    for (const auto id:{"accent_red","accent_green","accent_blue"}) {
        assert(r.Get(mod,id,&v)==SC_OK && v.real==0.5f);
    }
    assert(r.Get(mod,"background",&v)==SC_OK && v.real==0.8f);
    // A change only in a hidden channel still reveals the combined reset.
    v=real.default_value;v.real=0.75f;Set("accent_green",v);
    Frame();ClickReset();assert(r.Get(mod,"accent_green",&v)==SC_OK && v.real==0.5f);
    assert(r.Unregister(mod)==SC_OK);ImGui::DestroyContext();
    return 0;
}
