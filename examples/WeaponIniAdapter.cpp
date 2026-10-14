// Example adaptation of an existing INI-based weapon mod. Include this pattern
// in the mod's own ASI; keep its original initialization and INI path.
#include "sleekconfig.hpp"
#include <Windows.h>
#include <cstdio>
#include <cstring>

class WeaponIniAdapter {
public:
    void Start() {
        LoadOriginalIni(); // still works when SleekConfig is absent
        ScModDesc d{}; d.size=sizeof d; d.required_api=SC_API_VERSION;
        d.id="example.weapon.recoil"; d.name="Weapon Recoil Example";
        d.author="Example author"; d.version="1.0";
        d.description="Adjust recoil while keeping WeaponRecoil.ini.";
        d.persistence=SC_EXTERNAL; d.user=this;
        d.on_change=&Changed; d.on_load=&Loaded; d.on_save=&Saved;
        if (menu_.Open(d)!=SC_OK) return;
        ScSettingDesc s{};s.size=sizeof s;s.id="recoil_percent";
        s.label="Recoil strength";s.page="Weapons";s.group="Handling";
        s.type=SC_INT;s.minimum=0;s.maximum=200;s.step=5;
        s.default_value.type=SC_INT;s.default_value.integer=recoil_;
        menu_.Add(s);
    }
    void Stop() { menu_.Close(); }
    int RecoilPercent() const { return recoil_; }
private:
    void LoadOriginalIni() {
        recoil_=GetPrivateProfileIntA("weapons","recoil_percent",100,
            "WeaponRecoil.ini");
        if (recoil_<0) recoil_=0;if (recoil_>200) recoil_=200;
    }
    static int SC_CALL Changed(void* user,const char* id,const ScValue* v) {
        auto* self=static_cast<WeaponIniAdapter*>(user);
        if (std::strcmp(id,"recoil_percent")!=0) return 0;
        self->recoil_=v->integer; // game main thread, after render
        return 1;
    }
    static int SC_CALL Loaded(void* user) {
        auto* self=static_cast<WeaponIniAdapter*>(user);
        self->LoadOriginalIni();
        ScValue v{};v.type=SC_INT;v.integer=self->recoil_;
        return self->menu_.Api()->publish_value(self->menu_.Handle(),
            "recoil_percent",&v)==SC_OK;
    }
    static int SC_CALL Saved(void* user) {
        auto* self=static_cast<WeaponIniAdapter*>(user);
        char text[16];std::snprintf(text,sizeof text,"%d",self->recoil_);
        return WritePrivateProfileStringA("weapons","recoil_percent",text,
            "WeaponRecoil.ini")!=0;
    }
    int recoil_{100};
    sleekconfig::Registration menu_;
};
