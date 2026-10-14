#include "Menu.h"
#include "Registry.h"
#include "Persistence.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb/stb_image.h>
#include <cstdint>
#include <vector>

namespace sc {
namespace {
std::string ModuleDirectory() {
    HMODULE module{};
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ModuleDirectory),&module)) return {};
    char file[MAX_PATH]{};
    const DWORD length=GetModuleFileNameA(module,file,MAX_PATH);
    if (!length || length>=MAX_PATH) return {};
    std::string path(file,length);
    const auto slash=path.find_last_of("\\/");
    return slash==std::string::npos?std::string{}:path.substr(0,slash+1);
}
}
void Menu::ClearIcons() {
    for (auto& [_,icon]:icons_) if (icon.texture) icon.texture->Release();
    icons_.clear();
}

IDirect3DTexture9* Menu::IconFor(ScMod id,const Mod& mod) {
    if (!device_) return nullptr;
    const std::string relative=mod.icon.empty()?
        "SleekConfig/icons/"+mod.id+".png":mod.icon;
    auto& icon=icons_[id];
    if (icon.path==relative) return icon.texture;
    if (icon.texture) icon.texture->Release();
    icon={};
    icon.path=relative; // Cache failed loads too, so a missing icon is cheap to draw.
    if (!SafeRelativePath(relative)) return nullptr;
    std::vector<std::string> paths;
    if (mod.icon.empty()) {
        const auto moduleDir=ModuleDirectory();
        if (!moduleDir.empty()) paths.push_back(moduleDir+"icons/"+mod.id+".png");
    }
    paths.push_back(GamePath(relative));
    if (mod.icon.empty())
        paths.push_back(GamePath("modloader/SleekConfig/icons/"+mod.id+".png"));
    for (const auto& path:paths) {
        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (path.empty() || !GetFileAttributesExA(path.c_str(),GetFileExInfoStandard,&attributes) ||
            (attributes.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ||
            attributes.nFileSizeHigh || attributes.nFileSizeLow>4u*1024u*1024u)
            continue;
        int width=0,height=0,channels=0;
        if (!stbi_info(path.c_str(),&width,&height,&channels) ||
            width<1 || height<1 || width>512 || height>512) continue;
        unsigned char* pixels=stbi_load(path.c_str(),&width,&height,&channels,4);
        if (!pixels) continue;
        IDirect3DTexture9* texture=nullptr;
        const HRESULT created=device_->CreateTexture(width,height,1,0,D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,&texture,nullptr);
        if (SUCCEEDED(created) && texture) {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(texture->LockRect(0,&locked,nullptr,0))) {
                for (int y=0;y<height;++y) {
                    auto* row=static_cast<unsigned char*>(locked.pBits)+y*locked.Pitch;
                    const auto* source=pixels+y*width*4;
                    for (int x=0;x<width;++x) {
                        row[x*4+0]=source[x*4+2];
                        row[x*4+1]=source[x*4+1];
                        row[x*4+2]=source[x*4+0];
                        row[x*4+3]=source[x*4+3];
                    }
                }
                texture->UnlockRect(0);
                icon.texture=texture;icon.width=width;icon.height=height;
            } else texture->Release();
        }
        stbi_image_free(pixels);
        if (icon.texture) break;
    }
    return icon.texture;
}
}
