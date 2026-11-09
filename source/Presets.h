#pragma once
#include "sleekconfig.h"
#include <string>
#include <vector>

/* Per-mod presets: small text files that name the mod they belong to and carry
   that mod's values by setting ID. They can live in any folder under the
   game's modloader directory; SleekConfig saves new ones in presets/<mod-id>/
   beside its ASI. File format (UTF-8, tab separated, CRLF or LF):

     SLEEKCONFIG-PRESET\t1
     mod\t<mod-id>
     name\t<display name>
     set\t<setting-id>\t<ScType>\t<encoded value>

   Values use the same encoding as SleekConfig/<mod-id>.dat. Unknown header
   keys are ignored so later versions can add metadata. */
namespace sc {
struct Mod;
struct PresetInfo {
    std::string path;   // file path as found
    std::string modId;  // header "mod"
    std::string name;   // header "name", or the file name without .dat
    std::string source; // first folder below the scan root, shown in the list
    bool own{};         // saved by SleekConfig in its presets folder
};
struct PresetRow { std::string id; uint32_t type{}; std::string encoded; };
constexpr size_t kPresetNameMax=64;
std::string PresetDirectory();  // <ASI dir>\presets ("presets" in tests)
std::string PresetScanRoot();   // <game dir>\modloader ("modloader" in tests)
/* Reads only the header. False for anything that is not a readable preset. */
bool ReadPresetHeader(const std::string& path,PresetInfo& info);
/* Walks every root recursively (bounded) and returns the readable presets of
   every mod, sorted by mod and name. Never throws; broken files are skipped. */
std::vector<PresetInfo> ScanPresets(const std::vector<std::string>& roots);
/* Reads the value rows of a preset whose header names modId. Malformed rows
   are skipped and counted in skipped. */
ScResult ReadPreset(const std::string& path,const std::string& modId,
    std::vector<PresetRow>& rows,size_t& skipped,std::string& error);
/* Trims, drops control characters and limits the name to kPresetNameMax bytes. */
std::string CleanPresetName(const std::string& name);
/* Where SavePreset writes this name for this mod. */
std::string PresetPath(const std::string& modId,const std::string& name);
bool PresetFileExists(const std::string& path);
/* Writes the mod's current persisted values; replaces an existing file. */
ScResult SavePreset(const Mod& mod,const std::string& name,std::string& path,
    std::string& error);
}
