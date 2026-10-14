#include "Registry.h"
#include "Persistence.h"
#include <Windows.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <cmath>

static int calls=0;
static int saves=0;
void BindingTests();
static int SC_CALL OnChange(void*,const char*,const ScValue* v) {
    ++calls;return v->integer!=3;
}
static int SC_CALL OnSave(void*) {++saves;return 1;}
struct ExternalSource { ScMod mod{}; const char* ini{}; };
static int SC_CALL OnExternalLoad(void* user) {
    auto* source=static_cast<ExternalSource*>(user);
    char iniPath[MAX_PATH]{};
    if (!GetFullPathNameA(source->ini,MAX_PATH,iniPath,nullptr)) return 0;
    ScValue level{};level.type=SC_INT;
    level.integer=GetPrivateProfileIntA("settings","level",2,iniPath);
    ScValue speed{};speed.type=SC_INT;
    speed.integer=GetPrivateProfileIntA("settings","speed",1,iniPath);
    auto& registry=sc::Registry::Instance();
    return registry.Publish(source->mod,"level",&level)==SC_OK &&
        registry.Publish(source->mod,"speed",&speed)==SC_OK;
}
int main() {
    using namespace sc;
    auto& r=Registry::Instance();
    DeleteFileA("SleekConfig.dat");
    DeleteFileA(DataPath("test.mod").c_str());
    DeleteFileA(DataPath("test.other").c_str());
    DeleteFileA(DataPath("test.legacy").c_str());
    DeleteFileA(DataPath("test.migrated").c_str());
    {std::ofstream old("SleekConfig.dat",std::ios::binary);
        old<<"SLEEKCONFIG-DAT\t1\r\ntest.migrated\tlevel\t2\t8\r\n";}
    ScModDesc d{};d.size=sizeof d;d.required_api=1;d.id="test.mod";
    d.name="Test";d.persistence=SC_MANAGED_INI;
    d.ini_path="registry_test.ini";d.on_change=OnChange;
    ScMod m=0;assert(r.Register(&d,&m)==SC_OK);
    assert(r.SetMetadata(m,"pedrowav","Test description","1.2","HUD")==SC_OK);
    assert(r.Mods().at(m).author=="pedrowav");
    assert(r.Mods().at(m).description=="Test description");
    assert(r.Mods().at(m).version=="1.2");
    assert(r.Mods().at(m).category=="HUD");
    assert(r.Register(&d,&m)==SC_DUPLICATE);
    ScSettingDesc s{};s.size=sizeof s;s.id="level";s.label="Level";
    s.type=SC_INT;s.minimum=0;s.maximum=10;s.step=1;
    s.default_value.type=SC_INT;s.default_value.integer=2;
    assert(r.Add(m,&s)==SC_OK);
    assert(r.Add(m,&s)==SC_DUPLICATE);
    assert(GetFileAttributesA(DataPath("test.mod").c_str())==INVALID_FILE_ATTRIBUTES);
    ScValue v{};assert(r.Get(m,"level",&v)==SC_OK&&v.integer==2);
    v.integer=11;assert(r.Set(m,"level",&v)==SC_INVALID);
    v.integer=3;assert(r.Set(m,"level",&v)==SC_OK);
    r.Tick();assert(calls==1);
    assert(r.Get(m,"level",&v)==SC_OK&&v.integer==2);
    v.integer=5;assert(r.Set(m,"level",&v)==SC_OK);
    assert(r.Get(m,"level",&v)==SC_OK&&v.integer==2);
    r.Tick();assert(r.Get(m,"level",&v)==SC_OK&&v.integer==5);
    auto firstSave=r.Save(m);
    if (firstSave!=SC_OK) std::fprintf(stderr,"save=%d error=%s\n",firstSave,r.Error(m));
    assert(firstSave==SC_OK);
    ScValue early{};
    assert(ReadStoredValue("test.mod","level",early)==SC_OK);
    assert(early.type==SC_INT && early.integer==5);
    assert(GetFileAttributesA(DataPath("test.mod").c_str())!=INVALID_FILE_ATTRIBUTES);
    assert(GetFileAttributesA("registry_test.ini")==INVALID_FILE_ATTRIBUTES);
    ScModDesc other=d;other.id="test.other";other.ini_path=nullptr;
    other.persistence=SC_EXTERNAL;other.on_change=nullptr;other.on_save=OnSave;
    ScMod second{};assert(r.Register(&other,&second)==SC_OK);
    ScSettingDesc otherSetting=s;otherSetting.default_value.integer=4;
    assert(r.Add(second,&otherSetting)==SC_OK);
    assert(r.Save(second)==SC_OK);
    assert(saves==1);
    ScModDesc legacy=d;legacy.id="test.legacy";
    legacy.ini_path="legacy_test.ini";legacy.on_change=nullptr;
    ScMod third{};assert(r.Register(&legacy,&third)==SC_OK);
    assert(r.Add(third,&s)==SC_OK);
    {std::ofstream file("legacy_test.ini",std::ios::binary);
        file<<"[settings]\r\nlevel=6\r\n";}
    assert(r.Reload(third)==SC_OK);r.Tick();
    assert(r.Get(third,"level",&v)==SC_OK&&v.integer==6);
    assert(r.Save(third)==SC_OK);
    {
        std::ifstream file(DataPath("test.mod"),std::ios::binary);
        const std::string data((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        assert(data.find("test.mod\tlevel\t2\t5")!=std::string::npos);
        assert(data.find("test.other") == std::string::npos);
    }
    {
        std::ifstream file(DataPath("test.other"),std::ios::binary);
        const std::string data((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        assert(data.find("test.other\tlevel\t2\t4")!=std::string::npos);
    }
    {
        std::ifstream file(DataPath("test.legacy"),std::ios::binary);
        const std::string data((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        assert(data.find("test.legacy\tlevel\t2\t6")!=std::string::npos);
    }
    ScModDesc migrated=d;migrated.id="test.migrated";migrated.ini_path=nullptr;
    migrated.persistence=SC_EXTERNAL;migrated.on_change=nullptr;
    ScMod fourth{};assert(r.Register(&migrated,&fourth)==SC_OK);
    assert(r.Add(fourth,&s)==SC_OK);r.Tick();
    assert(r.Get(fourth,"level",&v)==SC_OK&&v.integer==8);
    ScSettingDesc color{};color.size=sizeof color;color.id="health_color";
    color.label="Health color";color.page="Colors";color.type=SC_COLOR;
    color.minimum=0;color.maximum=0xFFFFFF;color.step=1;
    color.default_value.type=SC_COLOR;color.default_value.integer=0xE74C3C;
    assert(r.Add(fourth,&color)==SC_OK);
    ScValue rgb{};rgb.type=SC_COLOR;rgb.integer=-1;
    assert(r.Set(fourth,"health_color",&rgb)==SC_INVALID);
    rgb.integer=0xABCDEF;
    assert(r.Set(fourth,"health_color",&rgb)==SC_OK);r.Tick();
    ScSettingDesc offset{};offset.size=sizeof offset;offset.id="offset_x";
    offset.label="Offset X";offset.type=SC_FLOAT;offset.minimum=-500;
    offset.maximum=500;offset.step=0.1;
    offset.default_value.type=SC_FLOAT;offset.default_value.real=0;
    assert(r.Add(fourth,&offset)==SC_OK);
    ScValue fractional{};fractional.type=SC_FLOAT;fractional.real=499.8f;
    assert(r.Set(fourth,"offset_x",&fractional)==SC_OK);r.Tick();
    assert(r.Save(fourth)==SC_OK);
    ScValue savedRgb{};
    assert(ReadStoredValue("test.migrated","health_color",savedRgb)==SC_OK);
    assert(savedRgb.type==SC_COLOR && savedRgb.integer==0xABCDEF);
    ScValue savedOffset{};
    assert(ReadStoredValue("test.migrated","offset_x",savedOffset)==SC_OK);
    assert(savedOffset.type==SC_FLOAT && std::abs(savedOffset.real-499.8f)<0.001f);
    assert(GetFileAttributesA(DataPath("test.migrated").c_str())!=INVALID_FILE_ATTRIBUTES);
    assert(GetFileAttributesA("SleekConfig.dat")!=INVALID_FILE_ATTRIBUTES);
    v.integer=7;assert(r.Set(m,"level",&v)==SC_OK);r.Tick();
    assert(r.Reload(m)==SC_OK);r.Tick();
    assert(r.Get(m,"level",&v)==SC_OK&&v.integer==5);
    v.integer=9;assert(r.Set(m,"level",&v)==SC_OK);r.Tick();
    HANDLE h=CreateFileA(DataPath("test.mod").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(h!=INVALID_HANDLE_VALUE);DWORD written=0;
    constexpr char external[]="test.mod\tnew_setting\t2\t8\r\n";
    assert(WriteFile(h,external,sizeof(external)-1,&written,nullptr));CloseHandle(h);
    assert(r.Save(second)==SC_OK);
    assert(r.Save(m)==SC_CONFLICT);
    assert(r.Reload(m)==SC_OK);r.Tick();
    assert(r.Save(m)==SC_OK);
    const int beforeUnload=calls;
    v.integer=4;assert(r.Set(m,"level",&v)==SC_OK);
    assert(r.Unregister(m)==SC_OK);
    assert(r.Unregister(second)==SC_OK);
    assert(r.Unregister(third)==SC_OK);
    assert(r.Unregister(fourth)==SC_OK);
    r.Tick();assert(calls==beforeUnload);
    assert(r.Get(m,"level",&v)==SC_NOT_FOUND);
    assert(r.Register(&d,&m)==SC_OK);
    assert(r.Add(m,&s)==SC_OK);
    r.Tick();assert(r.Get(m,"level",&v)==SC_OK&&v.integer==5);
    assert(r.Unregister(m)==SC_OK);
    // A managed INI supplies missing DAT values, including at first load.
    DeleteFileA(DataPath("test.priority").c_str());
    {std::ofstream ini("priority_test.ini",std::ios::binary);
        ini<<"[settings]\r\nlevel=7\r\nspeed=4\r\n";}
    ScModDesc managed=d;managed.id="test.priority";
    managed.ini_path="priority_test.ini";managed.on_change=nullptr;
    ScMod priority{};assert(r.Register(&managed,&priority)==SC_OK);
    ScSettingDesc speed=s;speed.id="speed";speed.label="Speed";
    speed.default_value.integer=1;
    assert(r.Add(priority,&s)==SC_OK);
    assert(r.Add(priority,&speed)==SC_OK);
    r.Tick();
    assert(r.Get(priority,"level",&v)==SC_OK&&v.integer==7);
    assert(r.Get(priority,"speed",&v)==SC_OK&&v.integer==4);
    {std::ofstream dat(DataPath("test.priority"),std::ios::binary);
        dat<<"SLEEKCONFIG-DAT\t1\r\ntest.priority\tlevel\t2\t8\r\n";}
    assert(r.Unregister(priority)==SC_OK);
    assert(r.Register(&managed,&priority)==SC_OK);
    assert(r.Add(priority,&s)==SC_OK);
    assert(r.Add(priority,&speed)==SC_OK);
    r.Tick();
    assert(r.Get(priority,"level",&v)==SC_OK&&v.integer==8);
    assert(r.Get(priority,"speed",&v)==SC_OK&&v.integer==4);
    DeleteFileA("priority_test.ini");
    assert(r.Reload(priority)==SC_OK);r.Tick();
    assert(r.Get(priority,"level",&v)==SC_OK&&v.integer==8);
    assert(r.Get(priority,"speed",&v)==SC_OK&&v.integer==1);
    DeleteFileA(DataPath("test.priority").c_str());
    const auto afterDatDeletion=r.Reload(priority);
    if (afterDatDeletion!=SC_OK) std::fprintf(stderr,"after delete=%d error=%s\n",
        afterDatDeletion,r.Error(priority));
    assert(afterDatDeletion==SC_OK);r.Tick();
    assert(r.Get(priority,"level",&v)==SC_OK&&v.integer==2);
    assert(r.Unregister(priority)==SC_OK);
    // External mods load their own INI; DAT only overrides matching keys.
    {std::ofstream ini("external_test.ini",std::ios::binary);
        ini<<"[settings]\r\nlevel=7\r\nspeed=4\r\n";}
    {std::ofstream dat(DataPath("test.external"),std::ios::binary);
        dat<<"SLEEKCONFIG-DAT\t1\r\ntest.external\tspeed\t2\t9\r\n";}
    ExternalSource source{0,"external_test.ini"};
    ScModDesc externalMod=other;externalMod.id="test.external";
    externalMod.on_save=nullptr;externalMod.on_load=OnExternalLoad;externalMod.user=&source;
    assert(r.Register(&externalMod,&source.mod)==SC_OK);
    assert(r.Add(source.mod,&s)==SC_OK);
    assert(r.Add(source.mod,&speed)==SC_OK);
    r.Tick();
    assert(r.Get(source.mod,"level",&v)==SC_OK&&v.integer==7);
    assert(r.Get(source.mod,"speed",&v)==SC_OK&&v.integer==9);
    DeleteFileA("external_test.ini");
    assert(r.Reload(source.mod)==SC_OK);r.Tick();
    assert(r.Get(source.mod,"level",&v)==SC_OK&&v.integer==2);
    assert(r.Get(source.mod,"speed",&v)==SC_OK&&v.integer==9);
    assert(r.Unregister(source.mod)==SC_OK);
    DeleteFileA(DataPath("test.external").c_str());
    DeleteFileA("SleekConfig.dat");
    DeleteFileA(DataPath("test.mod").c_str());
    DeleteFileA(DataPath("test.other").c_str());
    DeleteFileA(DataPath("test.legacy").c_str());
    DeleteFileA(DataPath("test.migrated").c_str());
    RemoveDirectoryA(DataDirectory().c_str());
    DeleteFileA("legacy_test.ini");
    BindingTests();
    RemoveDirectoryA(DataDirectory().c_str());
    return 0;
}
