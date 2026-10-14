#pragma once
#include "sleekconfig.h"
#include <Windows.h>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace sc {
struct Setting {
    std::string id,label,page,group,description,tooltip,options;
    uint32_t type{},flags{};
    ScValue initial{},value{},saved{};
    double minimum{},maximum{},step{};
    ScBindingDesc binding{};
    HMODULE bindingOwner{};
};
struct Mod {
    ScMod token{};
    std::string id,name,author,description,version,category,icon,dependencies,ini,error;
    uint32_t persistence{};
    ScChangeFn onChange{};
    ScPersistenceFn onLoad{},onSave{};
    void* user{};
    struct ModuleOwner { const void* address; HMODULE module; };
    std::vector<ModuleOwner> moduleOwners;
    std::vector<Setting> settings;
    uint64_t revision{};
    bool dirty{}, hasFileTime{}, startupPending{true}, published{};
    uint64_t fileTime{};
};
struct Pending { ScMod mod; std::string setting; ScValue value; bool fromSaved{}; };
class Registry {
public:
    static Registry& Instance();
    ScResult Register(const ScModDesc*,ScMod*);
    ScResult SetMetadata(ScMod,const char* author,const char* description,
        const char* version,const char* category);
    ScResult SetIcon(ScMod,const char* path);
    ScResult Unregister(ScMod);
    ScResult Add(ScMod,const ScSettingDesc*,const ScBindingDesc* binding=nullptr);
    ScResult Get(ScMod,const char*,ScValue*);
    ScResult Set(ScMod,const char*,const ScValue*);
    ScResult Publish(ScMod,const char*,const ScValue*);
    ScResult Save(ScMod);
    ScResult Reload(ScMod);
    ScResult Reset(ScMod);
    uint64_t Revision(ScMod) const;
    const char* Error(ScMod);
    void Tick();
    const std::map<ScMod,Mod>& Mods() const { return mods_; }
    void Queue(ScMod mod,const std::string& id,const ScValue& value);
    void QueueSaved(ScMod mod,const std::string& id,const ScValue& value);
    void Clear();
private:
    Registry()=default;
    Mod* Find(ScMod);
    Setting* Find(Mod&,const char*);
    ScResult Validate(const Setting&,const ScValue&) const;
    bool Live(const Mod&) const;
    std::map<ScMod,Mod> mods_;
    std::deque<Pending> pending_;
    ScMod nextToken_{1};
    bool inCallback_{};
};
}
