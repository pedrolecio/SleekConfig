#include "Registry.h"
#include "Persistence.h"
#include "Menu.h"
#include "plugin.h"
#include "CCamera.h"
#include <injector/injector.hpp>
#include <injector/hooking.hpp>
#include <Windows.h>
#include <d3d9.h>

using namespace plugin;
namespace {
sc::Menu menu;
// CGame::Process calls CPad::UpdatePads here. Run immediately afterward,
// before scripts, mods and the camera consume the new mouse state.
CdeclEvent<AddressList<0x53BEE6, H_CALL>, PRIORITY_AFTER, ArgPickNone, void()>
    afterPadUpdate;
injector::scoped_call cameraProcessCall;
uintptr_t previousCameraProcess{};
void __fastcall CameraProcessHook(CCamera* camera,void*) {
    if (!menu.IsOpen() && previousCameraProcess)
        reinterpret_cast<void(__thiscall*)(CCamera*)>(previousCameraProcess)(camera);
}
ScResult SC_CALL Register(const ScModDesc* d,ScMod* o) {return sc::Registry::Instance().Register(d,o);}
ScResult SC_CALL Unregister(ScMod m) {return sc::Registry::Instance().Unregister(m);}
ScResult SC_CALL Add(ScMod m,const ScSettingDesc* d) {return sc::Registry::Instance().Add(m,d);}
ScResult SC_CALL Get(ScMod m,const char* k,ScValue* v) {return sc::Registry::Instance().Get(m,k,v);}
ScResult SC_CALL Set(ScMod m,const char* k,const ScValue* v) {return sc::Registry::Instance().Set(m,k,v);}
ScResult SC_CALL Publish(ScMod m,const char* k,const ScValue* v) {return sc::Registry::Instance().Publish(m,k,v);}
ScResult SC_CALL Save(ScMod m) {return sc::Registry::Instance().Save(m);}
ScResult SC_CALL Reload(ScMod m) {return sc::Registry::Instance().Reload(m);}
ScResult SC_CALL Reset(ScMod m) {return sc::Registry::Instance().Reset(m);}
uint64_t SC_CALL Revision(ScMod m) {return sc::Registry::Instance().Revision(m);}
const char* SC_CALL Error(ScMod m) {return sc::Registry::Instance().Error(m);}
const ScApi api{sizeof(ScApi),SC_API_VERSION,
    SC_CAP_MANAGED_INI|SC_CAP_EXTERNAL_BACKEND|SC_CAP_CLEO_BRIDGE|SC_CAP_COLOR|SC_CAP_MOD_ICON|
        SC_CAP_VARIABLE_BINDING,
    Register,Unregister,Add,Get,
    Set,Save,Reload,Reset,Revision,Error,Publish};
class Runtime {
public:
    Runtime() {
        if (!plugin::IsGameVersion10us()) return;
        // Only the main CGame::Process callsite. Keeping its previous target
        // lets the game and other camera mods run normally while the menu is closed.
        previousCameraProcess=cameraProcessCall.install(0x53C104,
            reinterpret_cast<void*>(&CameraProcessHook)).as_int();
        Events::initRwEvent += [] {
            HWND window=RsGlobal.ps?static_cast<HWND>(RsGlobal.ps->window):nullptr;
            auto* device=static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
            menu.Initialize(window,device);
        };
        Events::shutdownRwEvent += [] {menu.Shutdown();};
        Events::d3dLostEvent += [] {menu.DeviceLost();};
        Events::d3dResetEvent += [] {menu.DeviceReset();};
        Events::gameProcessEvent.before += [] {menu.BeforeGameProcess();};
        afterPadUpdate += [] {menu.BlockGameMouse();};
        Events::gameProcessEvent += [] {sc::Registry::Instance().Tick();};
        Events::gameProcessEvent += [] {menu.ProcessActions();};
        Events::drawFontsEvent += [] {menu.Render();};
    }
} runtime;
}
extern "C" __declspec(dllexport) const ScApi* SC_CALL
SleekConfig_GetApi(uint32_t version,uint32_t size) {
    return plugin::IsGameVersion10us() && version==SC_API_VERSION && size==sizeof(ScApi)
        ? &api : nullptr;
}
extern "C" __declspec(dllexport) ScResult SC_CALL
SleekConfig_RegisterBoundSetting(ScMod mod,const ScSettingDesc* setting,
    const ScBindingDesc* binding) {
    if (!plugin::IsGameVersion10us()) return SC_UNSUPPORTED;
    if (!binding) return SC_INVALID;
    return sc::Registry::Instance().Add(mod,setting,binding);
}
/* Optional startup read for mods whose hooks must be configured before the
   first game-process callback. This does not change the version 1 API table. */
extern "C" __declspec(dllexport) ScResult SC_CALL
SleekConfig_ReadStored(const char* modId,const char* settingId,ScValue* value) {
    if (!plugin::IsGameVersion10us()) return SC_UNSUPPORTED;
    if (!modId || !settingId || !value || !*modId || !*settingId)
        return SC_INVALID;
    return sc::ReadStoredValue(modId,settingId,*value);
}
