#include "Registry.h"
#include "Persistence.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace sc {
namespace {
bool ValidId(const char* s) {
    if (!s || !*s || std::strlen(s)>96) return false;
    for (auto p=s; *p; ++p)
        if (!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||
              *p=='.'||*p=='_'||*p=='-')) return false;
    return true;
}
std::string Copy(const char* s) { return s ? std::string(s).substr(0,4096) : ""; }
bool HasNul(const ScValue& v) { return std::memchr(v.text,0,SC_TEXT_CAP)!=nullptr; }
size_t BindingBytes(uint32_t storage) {
    switch (storage) {
    case SC_STORAGE_BOOL8: return 1;
    case SC_STORAGE_INT32: return sizeof(int32_t);
    case SC_STORAGE_FLOAT: return sizeof(float);
    case SC_STORAGE_TEXT: return SC_TEXT_CAP;
    default: return 0;
    }
}
bool Writable(const ScBindingDesc& b) {
    const auto bytes=BindingBytes(b.storage);
    auto address=reinterpret_cast<uintptr_t>(b.variable);
    if (!address || !bytes || bytes>UINTPTR_MAX-address) return false;
    const auto end=address+bytes;
    while (address<end) {
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof(region)) ||
            region.State!=MEM_COMMIT || (region.Protect & PAGE_GUARD)) return false;
        const auto access=region.Protect & 0xFF;
        if (access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
            access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return false;
        const auto next=reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize;
        if (next<=address) return false;
        address=next;
    }
    return true;
}
bool BindingLive(const Setting& s) {
    if (!s.binding.variable || !s.bindingOwner) return true;
    HMODULE current{};
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<LPCSTR>(s.binding.variable),&current) && current==s.bindingOwner;
}
bool WriteBinding(const Setting& s,const ScValue& v) {
    if (!s.binding.variable) return true;
    if (!BindingLive(s) || !Writable(s.binding)) return false;
    switch (s.binding.storage) {
    case SC_STORAGE_BOOL8: {
        const uint8_t value=v.integer!=0;
        std::memcpy(s.binding.variable,&value,sizeof(value)); break;
    }
    case SC_STORAGE_INT32: std::memcpy(s.binding.variable,&v.integer,sizeof(v.integer)); break;
    case SC_STORAGE_FLOAT: std::memcpy(s.binding.variable,&v.real,sizeof(v.real)); break;
    case SC_STORAGE_TEXT: std::memcpy(s.binding.variable,v.text,SC_TEXT_CAP); break;
    default: return false;
    }
    return true;
}
}
Registry& Registry::Instance() { static Registry r; return r; }
Mod* Registry::Find(ScMod id) { auto it=mods_.find(id); return it==mods_.end()?nullptr:&it->second; }
Setting* Registry::Find(Mod& m,const char* id) {
    if (!id) return nullptr;
    for (auto& s:m.settings) if (s.id==id) return &s;
    return nullptr;
}
ScResult Registry::Register(const ScModDesc* d,ScMod* out) {
    if (d && d->required_api>SC_API_VERSION) return SC_UNSUPPORTED;
    if (inCallback_ || !d || !out || d->size<sizeof(ScModDesc) ||
        !ValidId(d->id) ||
        !d->name || !*d->name || d->persistence>SC_MANAGED_INI) return SC_INVALID;
    for (auto& [_,m]:mods_) if (m.id==d->id) return SC_DUPLICATE;
    Mod m; m.token=nextToken_++; m.id=d->id; m.name=Copy(d->name);
    m.author=Copy(d->author); m.description=Copy(d->description);
    m.version=Copy(d->version); m.category=Copy(d->category);
    m.icon=Copy(d->icon_path); m.dependencies=Copy(d->dependencies);
    m.ini=Copy(d->ini_path); m.persistence=d->persistence;
    m.onChange=d->on_change; m.onLoad=d->on_load;
    m.onSave=d->on_save; m.user=d->user;
    if (m.persistence==SC_MANAGED_INI && !SafeRelativePath(m.ini)) return SC_INVALID;
    if (m.persistence==SC_MANAGED_INI)
        for (const auto& [_,existing]:mods_)
            if (existing.persistence==SC_MANAGED_INI && _stricmp(existing.ini.c_str(),m.ini.c_str())==0)
                return SC_DUPLICATE;
    const void* callbacks[]={reinterpret_cast<const void*>(m.onChange),
        reinterpret_cast<const void*>(m.onLoad),reinterpret_cast<const void*>(m.onSave)};
    for (const void* callback:callbacks) if (callback) {
        HMODULE module{};
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(callback),&module)) return SC_INVALID;
        m.moduleOwners.push_back({callback,module});
    }
    *out=m.token; mods_.emplace(m.token,std::move(m)); return SC_OK;
}
ScResult Registry::SetMetadata(ScMod id,const char* author,
    const char* description,const char* version,const char* category) {
    if (inCallback_) return SC_INVALID;
    auto* m=Find(id); if (!m) return SC_NOT_FOUND;
    m->author=Copy(author); m->description=Copy(description);
    m->version=Copy(version); m->category=Copy(category);
    return SC_OK;
}
ScResult Registry::SetIcon(ScMod id,const char* path) {
    if (inCallback_ || !path || (*path && !SafeRelativePath(path))) return SC_INVALID;
    auto* m=Find(id); if (!m) return SC_NOT_FOUND;
    m->icon=Copy(path);return SC_OK;
}
ScResult Registry::Unregister(ScMod id) {
    if (inCallback_) return SC_INVALID;
    pending_.erase(std::remove_if(pending_.begin(),pending_.end(),
        [id](const Pending& p){return p.mod==id;}),pending_.end());
    auto it=mods_.find(id);if (it==mods_.end()) return SC_NOT_FOUND;
    mods_.erase(it);
    return SC_OK;
}
bool Registry::Live(const Mod& m) const {
    for (const auto& owner:m.moduleOwners) {
        HMODULE current{};
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(owner.address),&current) ||
            current!=owner.module) return false;
    }
    return true;
}
ScResult Registry::Validate(const Setting& s,const ScValue& v) const {
    if (v.type!=s.type || !HasNul(v)) return SC_INVALID;
    switch (s.type) {
    case SC_BOOL: return (v.integer==0||v.integer==1)?SC_OK:SC_INVALID;
    case SC_INT: case SC_ENUM: case SC_KEY:
        if (v.integer<s.minimum || v.integer>s.maximum) return SC_INVALID;
        if (s.step>0 && s.type==SC_INT &&
            std::abs((v.integer-s.minimum)/s.step-
                std::round((v.integer-s.minimum)/s.step))>0.00001) return SC_INVALID;
        return SC_OK;
    case SC_FLOAT:
        if (!std::isfinite(v.real) || v.real<s.minimum || v.real>s.maximum) return SC_INVALID;
        if (s.step>0 && std::abs((v.real-s.minimum)/s.step-
                std::round((v.real-s.minimum)/s.step))>0.001) return SC_INVALID;
        return SC_OK;
    case SC_COLOR: return v.integer>=0 && v.integer<=0xFFFFFF?SC_OK:SC_INVALID;
    case SC_STRING: case SC_BUTTON: return SC_OK;
    default: return SC_INVALID;
    }
}
ScResult Registry::Add(ScMod id,const ScSettingDesc* d,const ScBindingDesc* binding) {
    if (inCallback_ || !d || d->size<sizeof(ScSettingDesc) || !ValidId(d->id)
        || !d->label || !*d->label || d->type<SC_BOOL || d->type>SC_COLOR)
        return SC_INVALID;
    auto* m=Find(id); if (!m) return SC_NOT_FOUND;
    if (Find(*m,d->id)) return SC_DUPLICATE;
    Setting s; s.id=d->id; s.label=Copy(d->label); s.page=Copy(d->page);
    s.group=Copy(d->group); s.description=Copy(d->description);
    s.tooltip=Copy(d->tooltip); s.options=Copy(d->options);
    s.type=d->type; s.flags=d->flags; s.minimum=d->minimum;
    s.maximum=d->maximum; s.step=d->step;
    if (!std::isfinite(s.minimum)||!std::isfinite(s.maximum)||
        !std::isfinite(s.step)||s.minimum>s.maximum || s.step<0) return SC_INVALID;
    if ((s.type==SC_INT||s.type==SC_ENUM||s.type==SC_KEY) &&
        (s.minimum<std::numeric_limits<int32_t>::min() ||
         s.maximum>std::numeric_limits<int32_t>::max())) return SC_INVALID;
    if (s.type==SC_FLOAT &&
        (s.minimum< -std::numeric_limits<float>::max() ||
         s.maximum>std::numeric_limits<float>::max())) return SC_INVALID;
    if (s.type==SC_ENUM && s.maximum-s.minimum>255) return SC_INVALID;
    if (s.type==SC_ENUM && (s.options.empty() ||
        std::count(s.options.begin(),s.options.end(),'|')+1 !=
        static_cast<int>(s.maximum-s.minimum+1))) return SC_INVALID;
    s.initial=s.value=s.saved=d->default_value;
    if (s.type>=SC_BUTTON && s.type<=SC_DESCRIPTION) {
        s.initial.type=s.value.type=s.saved.type=s.type;
        s.initial.text[SC_TEXT_CAP-1]=s.value.text[SC_TEXT_CAP-1]=s.saved.text[SC_TEXT_CAP-1]=0; }
    else if (Validate(s,s.value)!=SC_OK) return SC_INVALID;
    if (binding) {
        if (binding->size<sizeof(ScBindingDesc) || !binding->variable) return SC_INVALID;
        const auto storage=binding->storage;
        const bool compatible=(storage==SC_STORAGE_BOOL8 && s.type==SC_BOOL) ||
            (storage==SC_STORAGE_INT32 && (s.type==SC_BOOL || s.type==SC_INT ||
                s.type==SC_ENUM || s.type==SC_KEY || s.type==SC_COLOR)) ||
            (storage==SC_STORAGE_FLOAT && s.type==SC_FLOAT) ||
            (storage==SC_STORAGE_TEXT && s.type==SC_STRING);
        const auto bytes=BindingBytes(storage);
        if (!compatible || (storage==SC_STORAGE_TEXT ? binding->capacity<bytes :
                binding->capacity!=bytes) || !Writable(*binding)) return SC_INVALID;
        s.binding=*binding;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCSTR>(binding->variable),&s.bindingOwner);
    }
    m->settings.push_back(std::move(s));
    if (!WriteBinding(m->settings.back(),m->settings.back().value)) {
        m->settings.pop_back();return SC_INVALID;
    }
    // Check each owning module once per tick, not every bound variable.
    const auto& added=m->settings.back();
    if (added.bindingOwner && std::none_of(m->moduleOwners.begin(),m->moduleOwners.end(),
            [&](const Mod::ModuleOwner& owner){return owner.module==added.bindingOwner;}))
        m->moduleOwners.push_back({added.binding.variable,added.bindingOwner});
    return SC_OK;
}
ScResult Registry::Get(ScMod id,const char* key,ScValue* out) {
    if (!out) return SC_INVALID;
    auto* m=Find(id); if (!m) return SC_NOT_FOUND;
    auto* s=Find(*m,key); if (!s) return SC_NOT_FOUND;
    *out=s->value; return SC_OK;
}
void Registry::Queue(ScMod id,const std::string& key,const ScValue& v) {
    pending_.push_back({id,key,v,false});
}
void Registry::QueueSaved(ScMod id,const std::string& key,const ScValue& v) {
    pending_.push_back({id,key,v,true});
}
ScResult Registry::Set(ScMod id,const char* key,const ScValue* v) {
    if (inCallback_ || !v) return SC_INVALID;
    auto* m=Find(id); if (!m) return SC_NOT_FOUND;
    auto* s=Find(*m,key); if (!s) return SC_NOT_FOUND;
    auto result=Validate(*s,*v); if (result!=SC_OK) return result;
    Queue(id,key,*v); return SC_OK;
}
ScResult Registry::Publish(ScMod id,const char* key,const ScValue* v) {
    if (inCallback_ || !v) return SC_INVALID;
    auto* m=Find(id); if (!m) return SC_NOT_FOUND;
    auto* s=Find(*m,key); if (!s) return SC_NOT_FOUND;
    auto r=Validate(*s,*v); if (r!=SC_OK) return r;
    if (!WriteBinding(*s,*v)) {m->error="Bound variable is no longer writable";return SC_INVALID;}
    s->value=s->saved=*v; ++m->revision; m->dirty=false;m->published=true;
    return SC_OK;
}
void Registry::Tick() {
    for (auto it=mods_.begin();it!=mods_.end();) {
        if (!Live(it->second)) {const auto id=it->first;++it;Unregister(id);}
        else ++it;
    }
    // A mod can register all settings and publish its own INI values before
    // the first tick. Load any remaining source, then apply DAT overrides last.
    std::vector<ScMod> initializing;
    for (const auto& [id,m]:mods_) if (m.startupPending) initializing.push_back(id);
    // Moving an empty MSVC deque still allocates replacement storage. Keep
    // checking module ownership above, but do no queue work on idle frames.
    if (initializing.empty() && pending_.empty()) return;
    auto previouslyQueued=std::move(pending_);pending_.clear();
    for (const auto id:initializing) {
        auto* m=Find(id);if (!m) continue;
        m->startupPending=false;
        std::vector<std::pair<std::string,ScValue>> stored;
        const auto refreshed=ReloadDat(m->id,m->error);
        for (const auto& s:m->settings) if (refreshed==SC_OK) {
            ScValue value{};
            const auto result=LoadDatValue(m->id,s,value);
            if (result==SC_OK && Validate(s,value)==SC_OK)
                stored.emplace_back(s.id,value);
            else if (result!=SC_NOT_FOUND)
                m->error="Invalid or unreadable mod data value for "+s.id;
        }
        if (m->persistence==SC_MANAGED_INI) LoadIni(*m,false);
        else if (!m->published && m->onLoad && !m->onLoad(m->user))
            m->error="Initial mod config load failed";
        for (const auto& [key,value]:stored) QueueSaved(id,key,value);
    }
    for (auto& item:previouslyQueued) pending_.push_back(std::move(item));
    auto work=std::move(pending_); pending_.clear();
    for (auto& p:work) {
        auto* m=Find(p.mod); if (!m) continue;
        if (!Live(*m)) {Unregister(p.mod);continue;}
        auto* s=Find(*m,p.setting.c_str()); if (!s || Validate(*s,p.value)!=SC_OK) continue;
        if (s->binding.variable && !Writable(s->binding)) {
            m->error="Bound variable is no longer writable";continue;
        }
        if (m->onChange) {
            inCallback_=true;
            const bool accept=m->onChange(m->user,s->id.c_str(),&p.value)!=0;
            inCallback_=false;
            if (!accept) { m->error="Change rejected by mod"; continue; }
        }
        if (!WriteBinding(*s,p.value)) {m->error="Bound variable is no longer writable";continue;}
        if (s->type!=SC_BUTTON) {s->value=p.value;++m->revision;
            if (p.fromSaved) {s->saved=p.value;m->dirty=false;}
            else m->dirty=true;}
        m->error.clear();
    }
}
ScResult Registry::Save(ScMod id) {
    auto* m=Find(id); if (!m || inCallback_) return m?SC_INVALID:SC_NOT_FOUND;
    if (!Live(*m)) {Unregister(id);return SC_NOT_FOUND;}
    ScResult r=CheckDatWritable(m->id,m->error);
    if (r!=SC_OK) return r;
    if (m->persistence==SC_EXTERNAL && m->onSave && !m->onSave(m->user))
        r=SC_IO_ERROR;
    if (r==SC_OK) r=SaveDat(*m,m->error);
    if (r==SC_OK) { for (auto& s:m->settings) s.saved=s.value;
        m->dirty=false; m->error.clear(); }
    else if (m->error.empty()) m->error="Save failed";
    return r;
}
ScResult Registry::Reload(ScMod id) {
    auto* m=Find(id); if (!m || inCallback_) return m?SC_INVALID:SC_NOT_FOUND;
    if (!Live(*m)) {Unregister(id);return SC_NOT_FOUND;}
    const auto refreshed=ReloadDat(m->id,m->error);
    if (refreshed!=SC_OK) return refreshed;
    std::vector<std::pair<std::string,ScValue>> storedValues;
    for (const auto& setting:m->settings) {
        ScValue stored{};
        const auto result=LoadDatValue(m->id,setting,stored);
        if (result==SC_NOT_FOUND) continue;
        if (result!=SC_OK || Validate(setting,stored)!=SC_OK) {
            m->error="Invalid or unreadable mod data value for "+setting.id;
            return result==SC_OK?SC_INVALID:result;
        }
        storedValues.emplace_back(setting.id,stored);
    }
    ScResult sourceResult=SC_OK;
    if (m->persistence==SC_MANAGED_INI) sourceResult=LoadIni(*m);
    else if (m->onLoad) {
        if (!m->onLoad(m->user)) {m->error="Reload failed";sourceResult=SC_IO_ERROR;}
    } else for (auto& s:m->settings) {
        if (!WriteBinding(s,s.saved)) {
            m->error="Bound variable is no longer writable";return SC_INVALID;
        }
        s.value=s.saved;
    }
    for (const auto& [key,value]:storedValues) QueueSaved(id,key,value);
    m->dirty=false;
    if (sourceResult==SC_OK) m->error.clear();
    return sourceResult;
}
ScResult Registry::Reset(ScMod id) {
    auto* m=Find(id); if (!m || inCallback_) return m?SC_INVALID:SC_NOT_FOUND;
    for (auto& s:m->settings) if (s.type<=SC_STRING || s.type==SC_KEY || s.type==SC_COLOR)
        Queue(id,s.id,s.initial);
    return SC_OK;
}
uint64_t Registry::Revision(ScMod id) const {
    auto it=mods_.find(id); return it==mods_.end()?0:it->second.revision;
}
const char* Registry::Error(ScMod id) { auto* m=Find(id); return m?m->error.c_str():"Unknown mod"; }
void Registry::Clear() {
    pending_.clear();
    mods_.clear();
}
}
