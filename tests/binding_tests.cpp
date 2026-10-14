#include "Registry.h"
#include "Persistence.h"
#include "../docs/PointerSettingsExample.hpp"
#include <cassert>
#include <cstring>
#include <fstream>

// Existing binaries request exactly this v1 table size. The optional export
// must not grow it or the descriptors used by native mods and the CLEO bridge.
static_assert(sizeof(ScApi)==56, "Win32 v1 ABI changed");
static_assert(sizeof(ScModDesc)==64, "Win32 mod descriptor ABI changed");
static_assert(sizeof(ScSettingDesc)==336, "Win32 setting descriptor ABI changed");
static int bindingCalls{};
static int32_t* rejectedVariable{};
static int32_t moduleNumber{};
static float moduleReal{};
static int SC_CALL LoadBoundSource(void* user) {
    const auto mod=*static_cast<ScMod*>(user);
    ScValue value{};value.type=SC_INT;value.integer=7;
    return sc::Registry::Instance().Publish(mod,"level",&value)==SC_OK;
}
static int SC_CALL RejectThree(void*,const char*,const ScValue* value) {
    ++bindingCalls;
    // Callback sees the old variable until it accepts.
    assert(*rejectedVariable!=value->integer);
    return value->integer!=3;
}

void BindingTests() {
    auto& r=sc::Registry::Instance();
    DeleteFileA(sc::DataPath("test.binding").c_str());
    DeleteFileA(sc::DataPath("test.binding.reject").c_str());
    CreateDirectoryA(sc::DataDirectory().c_str(),nullptr);
    ScModDesc mod{};mod.size=sizeof(mod);mod.required_api=1;
    mod.id="test.binding";mod.name="Binding";mod.persistence=SC_MANAGED_INI;
    mod.ini_path="binding_test.ini";
    ScMod handle{};assert(r.Register(&mod,&handle)==SC_OK);
    ScSettingDesc level{};level.size=sizeof(level);level.id="level";level.label="Level";
    level.type=SC_INT;level.default_value.type=SC_INT;level.default_value.integer=2;
    level.minimum=0;level.maximum=10;level.step=1;
    int32_t number=99;
    ScBindingDesc binding{sizeof(binding),SC_STORAGE_INT32,&number,sizeof(number)};
    auto bad=binding;bad.size=0;
    assert(r.Add(handle,&level,&bad)==SC_INVALID && number==99);
    bad=binding;bad.variable=nullptr;
    assert(r.Add(handle,&level,&bad)==SC_INVALID);
    bad=binding;bad.capacity=1;
    assert(r.Add(handle,&level,&bad)==SC_INVALID);
    bad=binding;bad.storage=SC_STORAGE_FLOAT;
    assert(r.Add(handle,&level,&bad)==SC_INVALID);
    bad=binding;bad.storage=99;
    assert(r.Add(handle,&level,&bad)==SC_INVALID);
    bad=binding;bad.variable=reinterpret_cast<void*>(1);
    assert(r.Add(handle,&level,&bad)==SC_INVALID);
    assert(r.Mods().at(handle).settings.empty());
    assert(r.Add(handle,&level,&binding)==SC_OK && number==2);
    number=6;assert(r.Add(handle,&level,&binding)==SC_DUPLICATE && number==6);
    number=2;
    auto moduleSetting=level;moduleSetting.id="module_level";
    binding.variable=&moduleNumber;
    assert(r.Add(handle,&moduleSetting,&binding)==SC_OK && moduleNumber==2);
    assert(r.Mods().at(handle).moduleOwners.size()==1);

    struct { uint8_t before=0xAA;bool value=false;uint8_t after=0xBB; } toggle;
    auto enabled=level;enabled.id="enabled";enabled.type=SC_BOOL;
    enabled.default_value.type=SC_BOOL;enabled.default_value.integer=1;enabled.maximum=1;
    binding={sizeof(binding),SC_STORAGE_BOOL8,&toggle.value,sizeof(bool)};
    assert(r.Add(handle,&enabled,&binding)==SC_OK && toggle.value);
    assert(toggle.before==0xAA && toggle.after==0xBB);
    int32_t bool32=8;
    enabled.id="enabled32";binding={sizeof(binding),SC_STORAGE_INT32,&bool32,sizeof(bool32)};
    assert(r.Add(handle,&enabled,&binding)==SC_OK && bool32==1);

    auto strength=level;strength.id="strength";strength.type=SC_FLOAT;
    strength.default_value={};strength.default_value.type=SC_FLOAT;
    strength.default_value.real=1.5f;strength.step=0;
    float real=99;
    binding={sizeof(binding),SC_STORAGE_FLOAT,&real,sizeof(real)};
    assert(r.Add(handle,&strength,&binding)==SC_OK && real==1.5f);
    auto moduleStrength=strength;moduleStrength.id="module_strength";
    binding.variable=&moduleReal;
    assert(r.Add(handle,&moduleStrength,&binding)==SC_OK && moduleReal==1.5f);
    assert(r.Mods().at(handle).moduleOwners.size()==1);

    auto text=level;text.id="name";text.type=SC_STRING;
    text.default_value={};text.default_value.type=SC_STRING;
    std::memcpy(text.default_value.text,"default",sizeof("default"));
    struct {char value[SC_TEXT_CAP];char sentinel='!';} name{};
    binding={sizeof(binding),SC_STORAGE_TEXT,name.value,SC_TEXT_CAP-1};
    assert(r.Add(handle,&text,&binding)==SC_INVALID);
    binding.capacity=SC_TEXT_CAP;
    assert(r.Add(handle,&text,&binding)==SC_OK && !std::strcmp(name.value,"default"));
    assert(name.sentinel=='!');

    int32_t color=0,choice=0,key=0;
    auto rgb=level;rgb.id="color";rgb.type=SC_COLOR;
    rgb.default_value.type=SC_COLOR;rgb.default_value.integer=0x3399FF;
    binding={sizeof(binding),SC_STORAGE_INT32,&color,sizeof(color)};
    assert(r.Add(handle,&rgb,&binding)==SC_OK && color==0x3399FF);
    auto preset=level;preset.id="preset";preset.type=SC_ENUM;preset.maximum=2;
    preset.default_value.type=SC_ENUM;preset.default_value.integer=1;preset.options="A|B|C";
    binding.variable=&choice;
    assert(r.Add(handle,&preset,&binding)==SC_OK && choice==1);
    auto hotkey=level;hotkey.id="key";hotkey.type=SC_KEY;hotkey.maximum=255;
    hotkey.default_value.type=SC_KEY;hotkey.default_value.integer=121;
    binding.variable=&key;
    assert(r.Add(handle,&hotkey,&binding)==SC_OK && key==121);
    auto button=level;button.id="button";button.type=SC_BUTTON;
    assert(r.Add(handle,&button,&binding)==SC_INVALID);

    {std::ofstream ini("binding_test.ini");ini<<"[settings]\nlevel=7\nstrength=2.5\n";}
    {std::ofstream dat(sc::DataPath(mod.id),std::ios::binary);
        dat<<"SLEEKCONFIG-DAT\t1\r\ntest.binding\tlevel\t2\t8\r\n";}
    r.Tick();assert(number==8 && real==2.5f); // DAT > INI > default
    ScValue v=level.default_value;v.integer=11;
    assert(r.Set(handle,"level",&v)==SC_INVALID && number==8);
    v.integer=5;assert(r.Set(handle,"level",&v)==SC_OK && number==8);
    r.Tick();assert(number==5);
    v=enabled.default_value;v.integer=0;
    assert(r.Set(handle,"enabled",&v)==SC_OK);
    assert(r.Set(handle,"enabled32",&v)==SC_OK);r.Tick();
    assert(!toggle.value && bool32==0 && toggle.before==0xAA && toggle.after==0xBB);
    v=text.default_value;std::memset(v.text,'a',SC_TEXT_CAP-1);v.text[SC_TEXT_CAP-1]=0;
    assert(r.Set(handle,"name",&v)==SC_OK);r.Tick();
    assert(std::strlen(name.value)==SC_TEXT_CAP-1 && name.sentinel=='!');
    v=rgb.default_value;v.integer=0xABCDEF;assert(r.Set(handle,"color",&v)==SC_OK);
    v=preset.default_value;v.integer=2;assert(r.Set(handle,"preset",&v)==SC_OK);
    v=hotkey.default_value;v.integer=122;assert(r.Set(handle,"key",&v)==SC_OK);
    r.Tick();assert(color==0xABCDEF && choice==2 && key==122);
    assert(r.Save(handle)==SC_OK);
    assert(r.Reset(handle)==SC_OK);r.Tick();
    assert(number==2 && real==1.5f && toggle.value && color==0x3399FF && choice==1 && key==121);
    assert(!std::strcmp(name.value,"default"));
    assert(r.Reload(handle)==SC_OK);r.Tick();
    assert(number==5 && real==2.5f && !toggle.value && color==0xABCDEF && choice==2 && key==122);
    v=level.default_value;v.integer=4;
    assert(r.Publish(handle,"level",&v)==SC_OK && number==4);
    v.integer=9;assert(r.Set(handle,"level",&v)==SC_OK);
    assert(r.Unregister(handle)==SC_OK);r.Tick();assert(number==4);

    // Callback-only settings can coexist with bound settings, with rejection
    // still controlling whether the pointer and committed value change.
    mod.id="test.binding.reject";mod.persistence=SC_EXTERNAL;mod.ini_path=nullptr;
    mod.on_change=RejectThree;
    assert(r.Register(&mod,&handle)==SC_OK);
    rejectedVariable=&number;binding={sizeof(binding),SC_STORAGE_INT32,&number,sizeof(number)};
    assert(r.Add(handle,&level,&binding)==SC_OK && number==2);r.Tick();
    v=level.default_value;v.integer=3;assert(r.Set(handle,"level",&v)==SC_OK);
    r.Tick();assert(number==2 && bindingCalls==1);
    ScValue current{};assert(r.Get(handle,"level",&current)==SC_OK && current.integer==2);
    v.integer=4;assert(r.Set(handle,"level",&v)==SC_OK);r.Tick();
    assert(number==4 && bindingCalls==2);
    mod.on_change=nullptr;
    auto unbound=level;unbound.id="unbound";assert(r.Add(handle,&unbound)==SC_OK);
    v.integer=5;assert(r.Set(handle,"unbound",&v)==SC_OK);r.Tick();
    assert(number==4 && bindingCalls==3);
    assert(r.Unregister(handle)==SC_OK);

    // An external on_load can publish into the pointer before DAT overrides.
    mod.id="test.binding.external";mod.on_load=LoadBoundSource;mod.user=&handle;
    {std::ofstream dat(sc::DataPath(mod.id),std::ios::binary);
        dat<<"SLEEKCONFIG-DAT\t1\r\ntest.binding.external\tlevel\t2\t9\r\n";}
    assert(r.Register(&mod,&handle)==SC_OK);
    binding.variable=&number;
    assert(r.Add(handle,&level,&binding)==SC_OK);r.Tick();assert(number==9);
    assert(r.Reset(handle)==SC_OK);r.Tick();assert(number==2);
    assert(r.Reload(handle)==SC_OK);r.Tick();assert(number==9);
    assert(r.Unregister(handle)==SC_OK);
    DeleteFileA(sc::DataPath(mod.id).c_str());mod.on_load=nullptr;mod.user=nullptr;

    // External reload with no loader/file restores the saved pointer too.
    mod.id="test.binding.empty";DeleteFileA(sc::DataPath(mod.id).c_str());
    assert(r.Register(&mod,&handle)==SC_OK);
    assert(r.Add(handle,&level,&binding)==SC_OK);r.Tick();
    v.integer=5;assert(r.Set(handle,"level",&v)==SC_OK);r.Tick();assert(number==5);
    assert(r.Reload(handle)==SC_OK && number==2);
    assert(r.Unregister(handle)==SC_OK);

    // A buffer made read-only is rejected without calling into the mod or
    // writing invalid memory. A failed registration reserves no setting ID.
    auto* page=static_cast<int32_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    assert(page);*page=99;
    DWORD oldProtect{};assert(VirtualProtect(page,4096,PAGE_READONLY,&oldProtect));
    assert(r.Register(&mod,&handle)==SC_OK);
    binding.variable=page;
    assert(r.Add(handle,&level,&binding)==SC_INVALID && *page==99);
    assert(VirtualProtect(page,4096,PAGE_READWRITE,&oldProtect));
    assert(r.Add(handle,&level,&binding)==SC_OK && *page==2);r.Tick();
    assert(VirtualProtect(page,4096,PAGE_READONLY,&oldProtect));
    v.integer=5;assert(r.Set(handle,"level",&v)==SC_OK);r.Tick();assert(*page==2);
    assert(r.Get(handle,"level",&current)==SC_OK && current.integer==2);
    assert(r.Publish(handle,"level",&v)==SC_INVALID);
    assert(r.Unregister(handle)==SC_OK);
    assert(VirtualFree(page,0,MEM_RELEASE));
    DeleteFileA("binding_test.ini");DeleteFileA(sc::DataPath("test.binding").c_str());
}
