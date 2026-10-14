#include "Registry.h"
#include "Persistence.h"
#define SLEEKCONFIG_BUILD
#include "sleekconfig_cleo.h"
#include "GameVersion.h"
#include <algorithm>
#include <cstring>
#include <climits>
#include <cmath>

/* CLEO 4/5 scripts reach these cdecl exports with standard 0AA2/0AA4/0AA7.
   No CLEO-private ABI or custom opcode number is required. */
namespace {
ScMod Full(int handle) { return handle>0?static_cast<ScMod>(handle):0; }
int Add(int h,const char* id,const char* label,const char* page,
    uint32_t type,int value,int min,int max) {
    if (!id||!label) return SC_INVALID;
    ScSettingDesc s{};s.size=sizeof s;s.id=id;s.label=label;
    s.page=page&&*page?page:"General";
    s.type=type;s.default_value.type=type;s.default_value.integer=value;
    s.minimum=min;s.maximum=max;s.step=1;
    return sc::Registry::Instance().Add(Full(h),&s);
}
int Register(const char* id,const char* name,const char* author,
    const char* description,const char* version,const char* category,
    const char* iniPath) {
    if (!plugin::IsGameVersion10us()) return 0;
    ScModDesc d{};d.size=sizeof d;d.required_api=SC_API_VERSION;
    d.id=id;d.name=name;d.author=author;d.description=description;
    d.version=version;d.category=category;
    if (iniPath) {d.persistence=SC_MANAGED_INI;d.ini_path=iniPath;}
    ScMod h=0;if (sc::Registry::Instance().Register(&d,&h)!=SC_OK) return 0;
    if (h>INT_MAX) {sc::Registry::Instance().Unregister(h);return 0;}
    return static_cast<int>(h);
}
}
extern "C" {
__declspec(dllexport) int SC_CALL SC_Cleo_Register(const char* id,const char* name) {
    return Register(id,name,nullptr,nullptr,nullptr,nullptr,nullptr);
}
__declspec(dllexport) int SC_CALL SC_Cleo_RegisterManaged(const char* id,
    const char* name,const char* iniPath) {
    return Register(id,name,nullptr,nullptr,nullptr,nullptr,iniPath);
}
__declspec(dllexport) int SC_CALL SC_Cleo_RegisterEx(const char* id,
    const char* name,const char* author,const char* description,
    const char* version,const char* category) {
    return Register(id,name,author,description,version,category,nullptr);
}
__declspec(dllexport) int SC_CALL SC_Cleo_RegisterManagedEx(const char* id,
    const char* name,const char* author,const char* description,
    const char* version,const char* category,const char* iniPath) {
    return Register(id,name,author,description,version,category,iniPath);
}
__declspec(dllexport) int SC_CALL SC_Cleo_SetMetadata(int h,
    const char* author,const char* description,const char* version,
    const char* category) {
    return sc::Registry::Instance().SetMetadata(Full(h),author,description,
        version,category);
}
__declspec(dllexport) int SC_CALL SC_Cleo_SetIcon(int h,const char* iconPath) {
    return sc::Registry::Instance().SetIcon(Full(h),iconPath);
}
__declspec(dllexport) int SC_CALL SC_Cleo_AddBool(int h,const char* id,const char* label,int value) {
    return Add(h,id,label,"General",SC_BOOL,value,0,1);
}
__declspec(dllexport) int SC_CALL SC_Cleo_AddInt(int h,const char* id,const char* label,int value,int min,int max) {
    return Add(h,id,label,"General",SC_INT,value,min,max);
}
__declspec(dllexport) int SC_CALL SC_Cleo_AddBoolPage(int h,const char* id,
    const char* label,const char* page,int value) {
    return Add(h,id,label,page,SC_BOOL,value,0,1);
}
__declspec(dllexport) int SC_CALL SC_Cleo_AddIntPage(int h,const char* id,
    const char* label,const char* page,int value,int min,int max) {
    return Add(h,id,label,page,SC_INT,value,min,max);
}
__declspec(dllexport) int SC_CALL SC_Cleo_AddEnumPage(int h,const char* id,
    const char* label,const char* page,const char* options,int value) {
    if (!id||!label||!options||!*options) return SC_INVALID;
    const int maximum=static_cast<int>(std::count(options,options+std::strlen(options),'|'));
    ScSettingDesc s{};s.size=sizeof s;s.id=id;s.label=label;
    s.page=page&&*page?page:"General";s.type=SC_ENUM;
    s.default_value.type=SC_ENUM;s.default_value.integer=value;
    s.minimum=0;s.maximum=maximum;s.step=1;s.options=options;
    return sc::Registry::Instance().Add(Full(h),&s);
}
__declspec(dllexport) int SC_CALL SC_Cleo_AddColorPage(int h,const char* id,
    const char* label,const char* page,int rgb) {
    return Add(h,id,label,page,SC_COLOR,rgb,0,0xFFFFFF);
}
__declspec(dllexport) int SC_CALL SC_Cleo_AddScaledFloatPage(int h,
    const char* id,const char* label,const char* page,
    int initial,int minimum,int maximum,int scale) {
    if (!id||!label||scale<1||scale>10000||minimum>maximum) return SC_INVALID;
    ScSettingDesc s{};s.size=sizeof s;s.id=id;s.label=label;
    s.page=page&&*page?page:"General";s.type=SC_FLOAT;
    s.default_value.type=SC_FLOAT;
    s.default_value.real=static_cast<float>(initial)/scale;
    s.minimum=static_cast<double>(minimum)/scale;
    s.maximum=static_cast<double>(maximum)/scale;
    s.step=1.0/scale;
    return sc::Registry::Instance().Add(Full(h),&s);
}
__declspec(dllexport) int SC_CALL SC_Cleo_GetScaledFloat(int h,
    const char* id,int scale) {
    if (!id||scale<1||scale>10000) return 0;
    ScValue v{};
    if (sc::Registry::Instance().Get(Full(h),id,&v)!=SC_OK ||
        v.type!=SC_FLOAT || !std::isfinite(v.real)) return 0;
    const double scaled=static_cast<double>(v.real)*scale;
    return scaled>=INT_MIN && scaled<=INT_MAX?
        static_cast<int>(std::lround(scaled)):0;
}
__declspec(dllexport) int SC_CALL SC_Cleo_GetInt(int h,const char* id) {
    ScValue v{};return sc::Registry::Instance().Get(Full(h),id,&v)==SC_OK?v.integer:0;
}
__declspec(dllexport) int SC_CALL SC_Cleo_ReadSavedInt(const char* modId,
    const char* settingId,int fallback) {
    if (!modId||!settingId) return fallback;
    ScValue value{};
    if (sc::ReadStoredValue(modId,settingId,value)!=SC_OK ||
        (value.type!=SC_BOOL && value.type!=SC_INT && value.type!=SC_ENUM &&
         value.type!=SC_KEY && value.type!=SC_COLOR)) return fallback;
    return value.integer;
}
__declspec(dllexport) int SC_CALL SC_Cleo_SetInt(int h,const char* id,int value) {
    ScValue v{};if (sc::Registry::Instance().Get(Full(h),id,&v)!=SC_OK) return SC_NOT_FOUND;
    if (v.type!=SC_BOOL&&v.type!=SC_INT&&v.type!=SC_ENUM&&
        v.type!=SC_KEY&&v.type!=SC_COLOR) return SC_INVALID;
    v.integer=value;return sc::Registry::Instance().Set(Full(h),id,&v);
}
__declspec(dllexport) int SC_CALL SC_Cleo_Revision(int h) {
    return static_cast<int>(sc::Registry::Instance().Revision(Full(h)) & 0x7fffffff);
}
__declspec(dllexport) int SC_CALL SC_Cleo_Unregister(int h) {
    return sc::Registry::Instance().Unregister(Full(h));
}
__declspec(dllexport) int SC_CALL SC_Cleo_Save(int h) {
    return sc::Registry::Instance().Save(Full(h));
}
__declspec(dllexport) int SC_CALL SC_Cleo_Reload(int h) {
    return sc::Registry::Instance().Reload(Full(h));
}
}
