#include "Persistence.h"
#include "Registry.h"
#include <Windows.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace sc {
namespace {
uint64_t Stamp(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExA(path.c_str(),GetFileExInfoStandard,&data)) return 0;
    ULARGE_INTEGER t; t.LowPart=data.ftLastWriteTime.dwLowDateTime;
    t.HighPart=data.ftLastWriteTime.dwHighDateTime; return t.QuadPart;
}
std::string Encode(const ScValue& value) {
    std::ostringstream out;
    if (value.type==SC_FLOAT) out<<std::setprecision(9)<<value.real;
    else if (value.type==SC_STRING) {
        static const char digits[]="0123456789ABCDEF";
        for (unsigned char c:std::string(value.text)) {
            out<<digits[c>>4]<<digits[c&15];
        }
    } else out<<value.integer;
    return out.str();
}
int Hex(char c) { if (c>='0'&&c<='9') return c-'0';
    if (c>='A'&&c<='F') return c-'A'+10;
    if (c>='a'&&c<='f') return c-'a'+10; return -1; }
bool Decode(ScValue& v,const std::string& s) {
    try {
        if (v.type==SC_STRING) {
            if (s.size()%2 || s.size()>=SC_TEXT_CAP*2) return false;
            std::string text; for (size_t i=0;i<s.size();i+=2) {
                int a=Hex(s[i]),b=Hex(s[i+1]); if (a<0||b<0||!(a*16+b)) return false;
                text.push_back(static_cast<char>(a*16+b));
            }
            std::copy(text.begin(),text.end(),v.text); v.text[text.size()]=0;
        } else if (v.type==SC_FLOAT) {
            size_t n=0; v.real=std::stof(s,&n); if (n!=s.size()) return false;
        } else { size_t n=0; v.integer=std::stoi(s,&n); if (n!=s.size()) return false; }
        return true;
    } catch (...) { return false; }
}

struct FileState {
    bool exists{};
    uint64_t modified{};
    uint64_t size{};
    bool operator==(const FileState& other) const {
        return exists==other.exists && modified==other.modified && size==other.size;
    }
};
bool ReadState(const std::string& path,FileState& state) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExA(path.c_str(),GetFileExInfoStandard,&data)) {
        const auto error=GetLastError();
        if (error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND) return false;
        state={}; return true;
    }
    ULARGE_INTEGER time{},size{};
    time.LowPart=data.ftLastWriteTime.dwLowDateTime;
    time.HighPart=data.ftLastWriteTime.dwHighDateTime;
    size.LowPart=data.nFileSizeLow;size.HighPart=data.nFileSizeHigh;
    state={true,time.QuadPart,size.QuadPart};return true;
}
struct DatRow { uint32_t type{};std::string encoded; };
bool Persisted(uint32_t type) {
    return (type>=SC_BOOL&&type<=SC_KEY)||type==SC_COLOR;
}
class DatStore;
DatStore& LegacyStore();
class DatStore {
public:
    explicit DatStore(std::string modId={},bool legacy=false)
        :modId_(std::move(modId)),legacy_(legacy) {}
    ScResult RawValue(const std::string& modId,const std::string& settingId,ScValue& value) {
        const auto result=Ensure();if (result!=SC_OK) return result;
        const auto it=rows_.find(modId+'\t'+settingId);
        if (it==rows_.end()) return SC_NOT_FOUND;
        value={};value.type=it->second.type;
        return Decode(value,it->second.encoded)?SC_OK:SC_INVALID;
    }
    ScResult Value(const std::string& modId,const Setting& setting,ScValue& value) {
        const auto result=Ensure();if (result!=SC_OK) return result;
        const auto it=rows_.find(modId+'\t'+setting.id);
        if (it==rows_.end()) return SC_NOT_FOUND;
        if (it->second.type!=setting.type) return SC_INVALID;
        value=setting.value;
        return Decode(value,it->second.encoded)?SC_OK:SC_INVALID;
    }
    ScResult Refresh(std::string& error) {
        if (!loaded_) {const auto result=Ensure();if (result!=SC_OK) error=error_;
            return result;}
        FileState current{};
        if (!ReadState(path_,current)) {error="Cannot inspect mod data file";
            return SC_IO_ERROR;}
        FileState oldLegacy{};
        if (watchLegacy_ && !ReadState(LegacyDataPath(),oldLegacy)) {
            error="Cannot inspect legacy SleekConfig.dat";return SC_IO_ERROR;
        }
        if (current==state_ && (!watchLegacy_ || oldLegacy==legacyState_)) {
            if (status_!=SC_OK) error=error_;
            return status_;}
        if (watchLegacy_) {auto& legacy=LegacyStore();legacy.loaded_=false;}
        rows_.clear();loaded_=false;status_=SC_OK;error_.clear();
        const auto result=Ensure();if (result!=SC_OK) error=error_;
        return result;
    }
    ScResult CheckWritable(std::string& error) {
        auto result=Ensure();
        if (result!=SC_OK) {error=error_;return result;}
        FileState current{};
        if (!ReadState(path_,current)) {error="Cannot inspect mod data file";return SC_IO_ERROR;}
        if (!(current==state_)) {
            error="Mod data file changed outside the game; reload before saving";
            return SC_CONFLICT;
        }
        if (watchLegacy_) {
            FileState legacy{};
            if (!ReadState(LegacyDataPath(),legacy)) {
                error="Cannot inspect legacy SleekConfig.dat";return SC_IO_ERROR;
            }
            if (!(legacy==legacyState_)) {
                error="Legacy SleekConfig.dat changed; reload before saving";
                return SC_CONFLICT;
            }
        }
        return SC_OK;
    }
    ScResult Save(const Mod& mod,std::string& error) {
        const auto result=CheckWritable(error);
        if (result!=SC_OK) return result;
        auto next=rows_;
        for (const auto& setting:mod.settings)
            if (Persisted(setting.type))
                next[mod.id+'\t'+setting.id]={setting.type,Encode(setting.value)};
        const auto directory=DataDirectory();
        if (directory.empty() ||
            (!CreateDirectoryA(directory.c_str(),nullptr) &&
             GetLastError()!=ERROR_ALREADY_EXISTS)) {
            error="Cannot create SleekConfig data directory";return SC_IO_ERROR;
        }
        const auto attributes=GetFileAttributesA(directory.c_str());
        if (attributes==INVALID_FILE_ATTRIBUTES ||
            (attributes&FILE_ATTRIBUTE_DIRECTORY)==0) {
            error="Cannot create SleekConfig data directory";return SC_IO_ERROR;
        }
        const std::string temp=path_+".tmp";
        {
            std::ofstream out(temp,std::ios::binary|std::ios::trunc);
            if (!out) {error="Cannot create temporary mod data file";return SC_IO_ERROR;}
            out<<"SLEEKCONFIG-DAT\t1\r\n";
            for (const auto& [key,row]:next)
                out<<key<<'\t'<<row.type<<'\t'<<row.encoded<<"\r\n";
            out.flush();
            if (!out) {out.close();DeleteFileA(temp.c_str());
                error="Cannot write temporary mod data file";return SC_IO_ERROR;}
        }
        if (!MoveFileExA(temp.c_str(),path_.c_str(),
                MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            DeleteFileA(temp.c_str());error="Cannot replace mod data file";
            return SC_IO_ERROR;
        }
        rows_=std::move(next);
        watchLegacy_=false;
        if (!ReadState(path_,state_)) {error="Cannot inspect saved mod data file";
            return SC_IO_ERROR;}
        return SC_OK;
    }
private:
    ScResult Ensure() {
        if (loaded_) return status_;
        rows_.clear();error_.clear();status_=SC_OK;
        loaded_=true;path_=legacy_?LegacyDataPath():DataPath(modId_);
        if (path_.empty()) {error_="Cannot resolve SleekConfig ASI directory";
            return status_=SC_IO_ERROR;}
        if (!ReadState(path_,state_)) {error_="Cannot inspect data file";
            return status_=SC_IO_ERROR;}
        if (!state_.exists) {
            if (!legacy_) {
                auto& old=LegacyStore();
                std::string legacyError;
                const auto oldStatus=old.Refresh(legacyError);
                if (oldStatus!=SC_OK) {error_=legacyError;return status_=oldStatus;}
                watchLegacy_=true;legacyState_=old.state_;
                if (old.state_.exists) {
                    const auto prefix=modId_+'\t';
                    for (const auto& [key,row]:old.rows_)
                        if (key.compare(0,prefix.size(),prefix)==0) rows_.emplace(key,row);
                }
            }
            return status_=SC_OK;
        }
        if (state_.size>4*1024*1024) {error_="Data file exceeds 4 MiB";
            return status_=SC_INVALID;}
        std::ifstream in(path_,std::ios::binary);
        if (!in) {error_="Cannot read data file";return status_=SC_IO_ERROR;}
        std::string line;
        if (!std::getline(in,line)) {error_="Data file is empty";
            return status_=SC_INVALID;}
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (line!="SLEEKCONFIG-DAT\t1") {error_="Unsupported SleekConfig.dat format";
            return status_=SC_INVALID;}
        size_t count=0;
        while (std::getline(in,line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (line.empty()) continue;
            const auto a=line.find('\t');
            const auto b=a==std::string::npos?a:line.find('\t',a+1);
            const auto c=b==std::string::npos?b:line.find('\t',b+1);
            if (a==std::string::npos || b==std::string::npos || c==std::string::npos ||
                a==0 || b==a+1 || ++count>100000) {
                error_="Invalid data file row";return status_=SC_INVALID;
            }
            const auto modId=line.substr(0,a),settingId=line.substr(a+1,b-a-1);
            const auto typeText=line.substr(b+1,c-b-1);
            if (modId.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")!=std::string::npos ||
                settingId.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")!=std::string::npos ||
                modId.size()>96 || settingId.size()>96 ||
                (!legacy_ && modId!=modId_)) {
                error_="Invalid data file identifier";return status_=SC_INVALID;
            }
            try {
                size_t used{};const auto type=std::stoul(typeText,&used);
                if (used!=typeText.size() || !Persisted(static_cast<uint32_t>(type)) ||
                    !rows_.emplace(modId+'\t'+settingId,
                        DatRow{static_cast<uint32_t>(type),line.substr(c+1)}).second) {
                    error_="Invalid data file value";return status_=SC_INVALID;
                }
            } catch (...) {error_="Invalid data file type";
                return status_=SC_INVALID;}
        }
        if (in.bad()) {error_="Cannot finish reading data file";
            return status_=SC_IO_ERROR;}
        return status_=SC_OK;
    }
    std::string modId_;
    bool legacy_{},watchLegacy_{},loaded_{};
    ScResult status_{SC_OK};
    std::string path_,error_;
    FileState state_{},legacyState_{};
    std::map<std::string,DatRow> rows_;
};
DatStore& LegacyStore() {static DatStore store({},true);return store;}
DatStore& Store(const std::string& modId) {
    static std::map<std::string,DatStore> stores;
    return stores.try_emplace(modId,modId,false).first->second;
}
}
bool SafeRelativePath(const std::string& path) {
    if (path.empty() || path.size()>240 || path[0]=='\\' || path[0]=='/' ||
        path.find(':')!=std::string::npos) return false;
    std::string part;
    for (char c:path) {
        if (c=='/'||c=='\\') {
            if (part.empty()||part==".."||part==".") return false;
            part.clear();
        } else part.push_back(c);
    }
    return !part.empty()&&part!=".."&&part!=".";
}
std::string GamePath(const std::string& relative) {
#ifdef SC_TEST
    return relative;
#else
    char executable[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr,executable,MAX_PATH)) return {};
    std::string path=executable;
    auto slash=path.find_last_of("\\/");
    if (slash==std::string::npos) return {};
    return path.substr(0,slash+1)+relative;
#endif
}
std::string DataDirectory() {
#ifdef SC_TEST
    return "SleekConfig";
#else
    HMODULE module{};
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&DataDirectory),&module)) return {};
    char path[MAX_PATH]{};
    const DWORD length=GetModuleFileNameA(module,path,MAX_PATH);
    if (!length || length>=MAX_PATH) return {};
    std::string directory(path,length);
    const auto slash=directory.find_last_of("\\/");
    if (slash==std::string::npos) return {};
    return directory.substr(0,slash+1)+"SleekConfig";
#endif
}
std::string LegacyDataPath() {
#ifdef SC_TEST
    return "SleekConfig.dat";
#else
    const auto directory=DataDirectory();
    const auto slash=directory.find_last_of("\\/");
    return slash==std::string::npos?std::string{}:
        directory.substr(0,slash+1)+"SleekConfig.dat";
#endif
}
std::string DataPath(const std::string& modId) {
    if (modId.empty() || modId.size()>96 ||
        modId.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")!=std::string::npos)
        return {};
    const auto directory=DataDirectory();
    return directory.empty()?std::string{}:directory+"\\"+modId+".dat";
}
ScResult LoadDatValue(const std::string& modId,const Setting& setting,ScValue& value) {
    if (DataPath(modId).empty()) return SC_INVALID;
    return Store(modId).Value(modId,setting,value);
}
ScResult ReadStoredValue(const std::string& modId,const std::string& settingId,ScValue& value) {
    if (DataPath(modId).empty() || settingId.empty() || settingId.size()>96 ||
        settingId.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")!=std::string::npos)
        return SC_INVALID;
    return Store(modId).RawValue(modId,settingId,value);
}
ScResult ReloadDat(const std::string& modId,std::string& error) {
    if (DataPath(modId).empty()) return SC_INVALID;
    return Store(modId).Refresh(error);
}
ScResult CheckDatWritable(const std::string& modId,std::string& error) {
    if (DataPath(modId).empty()) return SC_INVALID;
    return Store(modId).CheckWritable(error);
}
ScResult SaveDat(const Mod& mod,std::string& error) {
    return Store(mod.id).Save(mod,error);
}
ScResult SaveIni(Mod& mod) {
    const auto path=GamePath(mod.ini);
    if (path.empty()) {mod.error="Cannot resolve game directory";return SC_IO_ERROR;}
    const auto current=Stamp(path);
    if (!mod.hasFileTime && current!=0) {
        mod.error="Existing INI must be reloaded before first save";
        return SC_CONFLICT;
    }
    if (mod.hasFileTime && current!=mod.fileTime) {
        mod.error="INI changed outside the game; reload before saving";
        return SC_CONFLICT;
    }
    std::string temp=path+".tmp";
    {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        if (!out) { mod.error="Cannot create temporary INI";return SC_IO_ERROR; }
        out<<"; SleekConfig managed settings\r\n[settings]\r\n";
        for (auto& s:mod.settings) if (Persisted(s.type))
            out<<s.id<<"="<<Encode(s.value)<<"\r\n";
        out.flush(); if (!out) {mod.error="Cannot write temporary INI";return SC_IO_ERROR;}
    }
    if (!MoveFileExA(temp.c_str(),path.c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temp.c_str()); mod.error="Cannot replace INI"; return SC_IO_ERROR;
    }
    mod.fileTime=Stamp(path); mod.hasFileTime=true; return SC_OK;
}
ScResult LoadIni(Mod& mod,bool restoreDefaults) {
    const auto path=GamePath(mod.ini);
    if (path.empty()) {mod.error="Cannot resolve game directory";return SC_IO_ERROR;}
    std::ifstream in(path,std::ios::binary);
    if (!in) {
        const auto attrs=GetFileAttributesA(path.c_str());
        const auto missing=GetLastError();
        if (attrs==INVALID_FILE_ATTRIBUTES &&
            (missing==ERROR_FILE_NOT_FOUND||missing==ERROR_PATH_NOT_FOUND)) {
            if (restoreDefaults) for (auto& s:mod.settings) if (Persisted(s.type))
                Registry::Instance().QueueSaved(mod.token,s.id,s.initial);
            mod.fileTime=0; mod.hasFileTime=true; return SC_OK;
        }
        mod.error="Cannot read INI";return SC_IO_ERROR;
    }
    if (restoreDefaults) for (auto& s:mod.settings) if (Persisted(s.type))
        Registry::Instance().QueueSaved(mod.token,s.id,s.initial);
    std::string line; bool section=false; size_t invalid=0;
    while (std::getline(in,line)) {
        if (!line.empty()&&line.back()=='\r') line.pop_back();
        if (line=="[settings]") {section=true;continue;}
        if (!line.empty()&&line.front()=='[') {section=false;continue;}
        if (!section || line.empty() || line.front()==';') continue;
        auto pos=line.find('='); if (pos==std::string::npos) continue;
        auto key=line.substr(0,pos), raw=line.substr(pos+1);
        for (auto& s:mod.settings) if (s.id==key) {
            ScValue v=s.value;
            if (Decode(v,raw)) {
                if (s.type==SC_BOOL && (v.integer<0||v.integer>1)) ++invalid;
                else if (s.type==SC_FLOAT && (v.real<s.minimum||v.real>s.maximum)) ++invalid;
                else if (s.type!=SC_FLOAT && s.type!=SC_STRING &&
                    (v.integer<s.minimum||v.integer>s.maximum)) ++invalid;
                else Registry::Instance().QueueSaved(mod.token,s.id,v);
            } else ++invalid;
            break;
        }
    }
    if (in.bad()) {mod.error="INI read failed";return SC_IO_ERROR;}
    mod.fileTime=Stamp(path); mod.hasFileTime=true; mod.dirty=false;
    mod.error=invalid?std::to_string(invalid)+" invalid INI values ignored":"";
    return invalid?SC_INVALID:SC_OK;
}
}
