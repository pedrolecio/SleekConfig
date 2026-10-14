#pragma once
#include "sleekconfig.h"
#include <string>
namespace sc {
struct Mod;
struct Setting;
bool SafeRelativePath(const std::string& path);
std::string GamePath(const std::string& relative);
std::string DataDirectory();
std::string DataPath(const std::string& modId);
std::string LegacyDataPath();
ScResult LoadDatValue(const std::string& modId,const Setting& setting,ScValue& value);
ScResult ReadStoredValue(const std::string& modId,const std::string& settingId,ScValue& value);
ScResult ReloadDat(const std::string& modId,std::string& error);
ScResult CheckDatWritable(const std::string& modId,std::string& error);
ScResult SaveDat(const Mod& mod,std::string& error);
ScResult SaveIni(Mod& mod);
ScResult LoadIni(Mod& mod,bool restoreDefaults=true);
}
