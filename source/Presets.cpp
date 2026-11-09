#include "Presets.h"
#include "Persistence.h"
#include "Registry.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace sc {
namespace {
constexpr const char* kMagic="SLEEKCONFIG-PRESET\t1";
constexpr size_t kMaxPresetBytes=1024*1024;
constexpr size_t kHeaderBytes=4096;
constexpr size_t kMaxDirectories=60000;
constexpr size_t kMaxCandidates=20000;
constexpr int kMaxDepth=12;

bool ValidId(const std::string& id) {
    return !id.empty() && id.size()<=96 &&
        id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")==std::string::npos;
}
std::string Lower(std::string text) {
    std::transform(text.begin(),text.end(),text.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return text;
}
std::string FullPath(const std::string& path) {
    char buffer[MAX_PATH*2]{};
    const DWORD length=GetFullPathNameA(path.c_str(),sizeof buffer,buffer,nullptr);
    std::string full=length && length<sizeof buffer?std::string(buffer,length):path;
    std::replace(full.begin(),full.end(),'/','\\');
    while (full.size()>3 && full.back()=='\\') full.pop_back();
    return Lower(full);
}
bool EndsWithDat(const std::string& name) {
    return name.size()>4 && Lower(name.substr(name.size()-4))==".dat";
}
std::string Stem(const std::string& path) {
    const auto slash=path.find_last_of("\\/");
    std::string name=slash==std::string::npos?path:path.substr(slash+1);
    if (EndsWithDat(name)) name.resize(name.size()-4);
    return name;
}
/* Splits the text into lines without the line ending. */
std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start=0;
    while (start<=text.size()) {
        auto end=text.find('\n',start);
        if (end==std::string::npos) end=text.size();
        std::string line=text.substr(start,end-start);
        if (!line.empty() && line.back()=='\r') line.pop_back();
        lines.push_back(std::move(line));
        if (end==text.size()) break;
        start=end+1;
    }
    return lines;
}
bool ReadPrefix(const std::string& path,size_t limit,std::string& text,bool whole) {
    try {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExA(path.c_str(),GetFileExInfoStandard,&data) ||
            (data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || data.nFileSizeHigh ||
            data.nFileSizeLow>kMaxPresetBytes) return false;
        std::ifstream in(path,std::ios::binary);
        if (!in) return false;
        const size_t wanted=whole?static_cast<size_t>(data.nFileSizeLow):
            std::min<size_t>(limit,data.nFileSizeLow);
        text.assign(wanted,'\0');
        in.read(text.data(),static_cast<std::streamsize>(wanted));
        text.resize(static_cast<size_t>(in.gcount()));
        if (text.size()>=3 && static_cast<unsigned char>(text[0])==0xEF &&
            static_cast<unsigned char>(text[1])==0xBB &&
            static_cast<unsigned char>(text[2])==0xBF) text.erase(0,3);
        return !in.bad();
    } catch (...) {return false;}
}
/* Parses magic and header keys. Returns the index of the first value row. */
bool ParseHeader(const std::vector<std::string>& lines,PresetInfo& info,size_t& body) {
    if (lines.empty() || lines[0]!=kMagic) return false;
    info.modId.clear();info.name.clear();
    size_t i=1;
    for (;i<lines.size();++i) {
        const auto& line=lines[i];
        if (line.empty()) continue;
        const auto tab=line.find('\t');
        const auto key=line.substr(0,tab);
        if (key=="set") break;
        const auto value=tab==std::string::npos?std::string{}:line.substr(tab+1);
        if (key=="mod") info.modId=value;
        else if (key=="name") info.name=CleanPresetName(value);
        if (i>64) break;
    }
    body=i;
    return ValidId(info.modId);
}
std::string Clean(const std::string& text) {
    std::string out;
    for (char c:text) if (static_cast<unsigned char>(c)>=0x20 && c!=0x7F) out.push_back(c);
    return out;
}
std::string FileName(const std::string& name) {
    std::string out;
    for (unsigned char c:name) {
        if (std::isalnum(c) || c==' ' || c=='-' || c=='_' || c=='(' || c==')' || c=='.')
            out.push_back(static_cast<char>(c));
        else if (c<0x80 || (c&0xC0)==0xC0) out.push_back('_'); // one per UTF-8 character
    }
    while (!out.empty() && (out.back()=='.' || out.back()==' ')) out.pop_back();
    while (!out.empty() && (out.front()=='.' || out.front()==' ')) out.erase(0,1);
    if (out.size()>60) out.resize(60);
    if (out.empty()) out="preset";
    const auto stem=Lower(out.substr(0,out.find('.')));
    static const std::set<std::string> reserved{"con","prn","aux","nul",
        "com1","com2","com3","com4","com5","com6","com7","com8","com9",
        "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"};
    if (reserved.count(stem)) out="_"+out;
    return out;
}
bool EnsureDirectory(const std::string& path) {
    if (path.empty()) return false;
    const auto attributes=GetFileAttributesA(path.c_str());
    if (attributes!=INVALID_FILE_ATTRIBUTES) return (attributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
    const auto slash=path.find_last_of("\\/");
    if (slash!=std::string::npos && slash>0 && path[slash-1]!=':' &&
        !EnsureDirectory(path.substr(0,slash))) return false;
    return CreateDirectoryA(path.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS;
}
void Walk(const std::string& root,const std::string& ownDirectory,
    std::set<std::string>& seen,std::vector<PresetInfo>& out,
    size_t& directories,size_t& candidates) {
    struct Entry { std::string path; int depth; std::string source; };
    std::vector<Entry> stack{{root,0,{}}};
    while (!stack.empty() && directories<kMaxDirectories && candidates<kMaxCandidates) {
        const Entry entry=std::move(stack.back());stack.pop_back();++directories;
        WIN32_FIND_DATAA data{};
        const HANDLE find=FindFirstFileExA((entry.path+"\\*").c_str(),FindExInfoBasic,&data,
            FindExSearchNameMatch,nullptr,FIND_FIRST_EX_LARGE_FETCH);
        if (find==INVALID_HANDLE_VALUE) continue;
        do {
            const std::string name=data.cFileName;
            if (name=="." || name=="..") continue;
            const std::string path=entry.path+"\\"+name;
            const std::string source=entry.depth==0?name:entry.source;
            if (data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) {
                if (entry.depth<kMaxDepth) stack.push_back({path,entry.depth+1,source});
                continue;
            }
            if (!EndsWithDat(name) || data.nFileSizeHigh || data.nFileSizeLow<20 ||
                data.nFileSizeLow>kMaxPresetBytes) continue;
            if (++candidates>kMaxCandidates) break;
            const auto key=FullPath(path);
            if (!seen.insert(key).second) continue;
            PresetInfo info;
            if (!ReadPresetHeader(path,info)) continue;
            info.own=!ownDirectory.empty() && key.compare(0,ownDirectory.size(),ownDirectory)==0 &&
                key.size()>ownDirectory.size() && key[ownDirectory.size()]=='\\';
            info.source=info.own?"Saved by you":(entry.depth==0?"modloader":source);
            out.push_back(std::move(info));
        } while (FindNextFileA(find,&data));
        FindClose(find);
    }
}
}
std::string PresetDirectory() {
#ifdef SC_TEST
    return "presets";
#else
    const auto directory=AsiDirectory();
    return directory.empty()?std::string{}:directory+"presets";
#endif
}
std::string PresetScanRoot() {
#ifdef SC_TEST
    return "modloader";
#else
    return GamePath("modloader");
#endif
}
std::string CleanPresetName(const std::string& name) {
    std::string text=Clean(name);
    const auto first=text.find_first_not_of(' ');
    if (first==std::string::npos) return {};
    text=text.substr(first,text.find_last_not_of(' ')-first+1);
    if (text.size()>kPresetNameMax) {
        size_t cut=kPresetNameMax;
        while (cut>0 && (static_cast<unsigned char>(text[cut])&0xC0)==0x80) --cut;
        text.resize(cut);
        while (!text.empty() && text.back()==' ') text.pop_back();
    }
    return text;
}
bool ReadPresetHeader(const std::string& path,PresetInfo& info) {
    try {
        std::string text;
        if (!ReadPrefix(path,kHeaderBytes,text,false)) return false;
        auto lines=Lines(text);
        // The last line may be cut by the header limit; it is never needed.
        if (text.size()>=kHeaderBytes && lines.size()>1) lines.pop_back();
        size_t body{};
        PresetInfo parsed;
        if (!ParseHeader(lines,parsed,body)) return false;
        parsed.path=path;
        if (parsed.name.empty()) parsed.name=CleanPresetName(Stem(path));
        if (parsed.name.empty()) parsed.name="Preset";
        info=std::move(parsed);
        return true;
    } catch (...) {return false;}
}
std::vector<PresetInfo> ScanPresets(const std::vector<std::string>& roots) {
    std::vector<PresetInfo> out;
    try {
        std::set<std::string> seen;
        size_t directories=0,candidates=0;
        const auto own=PresetDirectory().empty()?std::string{}:FullPath(PresetDirectory());
        for (const auto& root:roots) {
            if (root.empty()) continue;
            const auto attributes=GetFileAttributesA(root.c_str());
            if (attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY))
                continue;
            Walk(root,own,seen,out,directories,candidates);
        }
        std::sort(out.begin(),out.end(),[](const PresetInfo& a,const PresetInfo& b) {
            if (a.modId!=b.modId) return a.modId<b.modId;
            const auto left=Lower(a.name),right=Lower(b.name);
            if (left!=right) return left<right;
            if (a.own!=b.own) return a.own;
            return Lower(a.path)<Lower(b.path);
        });
    } catch (...) {}
    return out;
}
ScResult ReadPreset(const std::string& path,const std::string& modId,
    std::vector<PresetRow>& rows,size_t& skipped,std::string& error) {
    rows.clear();skipped=0;
    try {
        std::string text;
        if (!ReadPrefix(path,0,text,true)) {error="Cannot read preset file";return SC_IO_ERROR;}
        const auto lines=Lines(text);
        PresetInfo info;size_t body{};
        if (!ParseHeader(lines,info,body)) {error="Not a SleekConfig preset";return SC_INVALID;}
        if (info.modId!=modId) {error="Preset belongs to another mod ("+info.modId+")";
            return SC_INVALID;}
        std::set<std::string> ids;
        for (size_t i=body;i<lines.size();++i) {
            const auto& line=lines[i];
            if (line.empty()) continue;
            const auto a=line.find('\t');
            const auto b=a==std::string::npos?a:line.find('\t',a+1);
            const auto c=b==std::string::npos?b:line.find('\t',b+1);
            if (line.compare(0,a,"set")!=0) continue; // unknown keys are metadata
            if (a==std::string::npos || b==std::string::npos || c==std::string::npos) {
                ++skipped;continue;}
            PresetRow row;
            row.id=line.substr(a+1,b-a-1);
            const auto typeText=line.substr(b+1,c-b-1);
            row.encoded=line.substr(c+1);
            if (!ValidId(row.id) || typeText.empty() || typeText.size()>3 ||
                typeText.find_first_not_of("0123456789")!=std::string::npos) {++skipped;continue;}
            row.type=static_cast<uint32_t>(std::stoul(typeText));
            if (!PersistedType(row.type) || !ids.insert(row.id).second) {++skipped;continue;}
            rows.push_back(std::move(row));
        }
        return SC_OK;
    } catch (...) {
        rows.clear();error="Cannot read preset file";return SC_IO_ERROR;
    }
}
std::string PresetPath(const std::string& modId,const std::string& name) {
    const auto directory=PresetDirectory();
    const auto clean=CleanPresetName(name);
    if (directory.empty() || !ValidId(modId) || clean.empty()) return {};
    return directory+"\\"+modId+"\\"+FileName(clean)+".dat";
}
bool PresetFileExists(const std::string& path) {
    const auto attributes=GetFileAttributesA(path.c_str());
    return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
}
ScResult SavePreset(const Mod& mod,const std::string& name,std::string& path,
    std::string& error) {
    const auto clean=CleanPresetName(name);
    if (clean.empty()) {error="Enter a preset name";return SC_INVALID;}
    path=PresetPath(mod.id,clean);
    if (path.empty()) {error="Cannot resolve the preset folder";return SC_IO_ERROR;}
    const auto slash=path.find_last_of("\\/");
    if (!EnsureDirectory(path.substr(0,slash))) {
        error="Cannot create the preset folder";return SC_IO_ERROR;}
    try {
        const std::string temp=path+".tmp";
        {
            std::ofstream out(temp,std::ios::binary|std::ios::trunc);
            if (!out) {error="Cannot create the preset file";return SC_IO_ERROR;}
            out<<kMagic<<"\r\n";
            out<<"mod\t"<<mod.id<<"\r\n";
            out<<"name\t"<<clean<<"\r\n";
            out<<"mod_name\t"<<Clean(mod.name)<<"\r\n";
            if (!mod.version.empty()) out<<"mod_version\t"<<Clean(mod.version)<<"\r\n";
            for (const auto& setting:mod.settings)
                if (PersistedType(setting.type))
                    out<<"set\t"<<setting.id<<'\t'<<setting.type<<'\t'
                       <<EncodeValue(setting.value)<<"\r\n";
            out.flush();
            if (!out) {out.close();DeleteFileA(temp.c_str());
                error="Cannot write the preset file";return SC_IO_ERROR;}
        }
        if (!MoveFileExA(temp.c_str(),path.c_str(),
                MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            DeleteFileA(temp.c_str());error="Cannot replace the preset file";
            return SC_IO_ERROR;
        }
    } catch (...) {error="Cannot write the preset file";return SC_IO_ERROR;}
    return SC_OK;
}
}
