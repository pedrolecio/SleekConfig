#include "Presets.h"
#include "Registry.h"
#include "Persistence.h"
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
void Write(const std::string& path,const std::string& text) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out<<text;
}
const sc::PresetInfo* Find(const std::vector<sc::PresetInfo>& list,const char* modId,
    const char* name) {
    for (const auto& preset:list)
        if (preset.modId==modId && preset.name==name) return &preset;
    return nullptr;
}
}
void PresetTests() {
    using namespace sc;
    std::error_code ignored;
    std::filesystem::remove_all("modloader",ignored);
    std::filesystem::remove_all("presets",ignored);
    auto& r=Registry::Instance();
    ScModDesc d{};d.size=sizeof d;d.required_api=1;d.id="test.preset";d.name="Preset Mod";
    d.version="2.1";ScMod mod{};assert(r.Register(&d,&mod)==SC_OK);
    ScSettingDesc level{};level.size=sizeof level;level.id="level";level.label="Level";
    level.type=SC_INT;level.maximum=100;level.default_value.type=SC_INT;level.default_value.integer=5;
    assert(r.Add(mod,&level)==SC_OK);
    ScSettingDesc strength=level;strength.id="strength";strength.type=SC_FLOAT;strength.maximum=1;
    strength.step=0.05;strength.default_value={};strength.default_value.type=SC_FLOAT;
    strength.default_value.real=0.5f;assert(r.Add(mod,&strength)==SC_OK);
    ScSettingDesc text=level;text.id="text";text.type=SC_STRING;text.default_value={};
    text.default_value.type=SC_STRING;strcpy_s(text.default_value.text,"tab\tand ação");
    assert(r.Add(mod,&text)==SC_OK);
    ScSettingDesc header=level;header.id="title";header.type=SC_HEADER;
    assert(r.Add(mod,&header)==SC_OK);
    r.Tick();
    ScValue v{};v.type=SC_INT;v.integer=42;assert(r.Set(mod,"level",&v)==SC_OK);r.Tick();

    // Names are trimmed, control characters removed and the length bounded.
    assert(CleanPresetName("  Night\tdrive \r\n")=="Nightdrive");
    assert(CleanPresetName("   ").empty());
    assert(CleanPresetName(std::string(100,'x')).size()==kPresetNameMax);
    assert(PresetPath("test.preset","con")=="presets\\test.preset\\_con.dat");
    assert(PresetPath("test.preset","a/b:c?")=="presets\\test.preset\\a_b_c_.dat");
    assert(PresetPath("test.preset","   ").empty());
    assert(PresetPath("Bad Id","x").empty());

    std::string path,error;
    const auto& owner=r.Mods().at(mod);
    assert(SavePreset(owner,"  ",path,error)==SC_INVALID);
    assert(SavePreset(owner,"Mine: ação",path,error)==SC_OK);
    assert(path=="presets\\test.preset\\Mine_ a__o.dat" && PresetFileExists(path));
    PresetInfo info;
    assert(ReadPresetHeader(path,info) && info.modId=="test.preset" && info.name=="Mine: ação");
    std::vector<PresetRow> rows;size_t skipped=0;
    assert(ReadPreset(path,"test.preset",rows,skipped,error)==SC_OK && skipped==0);
    // Only persisted types are written, keyed by setting id.
    assert(rows.size()==3);
    for (const auto& row:rows) {
        ScValue value{};value.type=row.type;assert(DecodeValue(value,row.encoded));
        if (row.id=="level") assert(value.integer==42);
        else if (row.id=="strength") assert(value.real==0.5f);
        else if (row.id=="text") assert(std::strcmp(value.text,"tab\tand ação")==0);
        else assert(false);
    }
    assert(ReadPreset(path,"test.other",rows,skipped,error)==SC_INVALID && rows.empty());
    // Saving the same name replaces the file in place.
    v.integer=43;assert(r.Set(mod,"level",&v)==SC_OK);r.Tick();
    assert(SavePreset(r.Mods().at(mod),"Mine: ação",path,error)==SC_OK);
    assert(ReadPreset(path,"test.preset",rows,skipped,error)==SC_OK);
    assert(std::any_of(rows.begin(),rows.end(),[](const PresetRow& row){
        return row.id=="level" && row.encoded=="43";}));

    // Presets shipped anywhere below modloader, including deep folders.
    Write("modloader\\Shipper\\data\\presets\\night.dat",
        "\xEF\xBB\xBFSLEEKCONFIG-PRESET\t1\nmod\ttest.preset\nname\tNight\nfuture\tkey\n"
        "set\tlevel\t2\t77\nset\tstrength\t3\t0.25\nset\tmissing\t2\t1\n"
        "set\tbroken\nset\tLEVEL\t2\t1\nset\tlevel\t2\t9\nset\ttext\t99\t00\n");
    Write("modloader\\Other\\other.dat","SLEEKCONFIG-PRESET\t1\r\nmod\ttest.other\r\nname\tOther\r\n");
    Write("modloader\\Unnamed\\Fallback Name.dat","SLEEKCONFIG-PRESET\t1\nmod\ttest.preset\n");
    Write("modloader\\Broken\\garbage.dat",std::string("\0\x01\x02 not a preset at all",24));
    Write("modloader\\Broken\\nomod.dat","SLEEKCONFIG-PRESET\t1\nname\tNo mod\nset\tlevel\t2\t1\n");
    Write("modloader\\Broken\\badid.dat","SLEEKCONFIG-PRESET\t1\nmod\tBad Id\n");
    Write("modloader\\Broken\\version.dat","SLEEKCONFIG-PRESET\t2\nmod\ttest.preset\n");
    Write("modloader\\Game\\handling.dat","; game data file\nLANDSTAL 1700.0 5008.3\n");
    Write("modloader\\Game\\empty.dat","");
    Write("modloader\\Game\\readme.txt","SLEEKCONFIG-PRESET\t1\nmod\ttest.preset\n");
    // A preset that is locked by another process cannot be read and is skipped.
    Write("modloader\\Locked\\locked.dat","SLEEKCONFIG-PRESET\t1\nmod\ttest.preset\nname\tLocked\n");
    const HANDLE locked=CreateFileA("modloader\\Locked\\locked.dat",GENERIC_READ,0,nullptr,
        OPEN_EXISTING,0,nullptr);
    assert(locked!=INVALID_HANDLE_VALUE);

    const auto found=ScanPresets({PresetScanRoot(),PresetDirectory()});
    CloseHandle(locked);
    const auto* mine=Find(found,"test.preset","Mine: ação");
    const auto* night=Find(found,"test.preset","Night");
    const auto* fallback=Find(found,"test.preset","Fallback Name");
    assert(mine && mine->own && mine->source=="Saved by you");
    assert(night && !night->own && night->source=="Shipper");
    assert(fallback && fallback->source=="Unnamed");
    assert(Find(found,"test.other","Other"));
    assert(!Find(found,"test.preset","Locked") && !Find(found,"test.preset","No mod"));
    const auto forMod=std::count_if(found.begin(),found.end(),
        [](const PresetInfo& p){return p.modId=="test.preset";});
    assert(forMod==3);
    // The own folder is listed once even when it is also below the scan root.
    std::filesystem::create_directories("modloader\\SleekConfig");
    assert(ScanPresets({"presets","presets"}).size()==1);

    assert(ReadPreset(night->path,"test.preset",rows,skipped,error)==SC_OK);
    // level, strength and missing are kept; broken, LEVEL, the duplicate level
    // and the unsupported type are skipped.
    assert(rows.size()==3 && skipped==4);
    assert(rows[0].id=="level" && rows[0].encoded=="77");
    assert(ReadPreset("modloader\\Broken\\garbage.dat","test.preset",rows,skipped,error)!=SC_OK);
    assert(ReadPreset("modloader\\missing.dat","test.preset",rows,skipped,error)==SC_IO_ERROR);
    // Missing roots are ignored.
    assert(ScanPresets({"does-not-exist",""}).empty());

    assert(r.Unregister(mod)==SC_OK);
    std::filesystem::remove_all("modloader",ignored);
    std::filesystem::remove_all("presets",ignored);
}
