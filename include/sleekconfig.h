#ifndef SLEEKCONFIG_H
#define SLEEKCONFIG_H
#include <stdint.h>
#ifdef _WIN32
#define SC_CALL __cdecl
#else
#define SC_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define SC_API_VERSION 1u
#define SC_TEXT_CAP 256u
#define SC_CAP_MANAGED_INI 1u
#define SC_CAP_EXTERNAL_BACKEND 2u
#define SC_CAP_CLEO_BRIDGE 4u
#define SC_CAP_COLOR 8u
#define SC_CAP_MOD_ICON 16u
#define SC_CAP_VARIABLE_BINDING 32u
typedef uint64_t ScMod;
typedef enum ScResult { SC_OK=0, SC_NOT_FOUND=1, SC_INVALID=2,
    SC_DUPLICATE=3, SC_REJECTED=4, SC_IO_ERROR=5, SC_CONFLICT=6,
    SC_UNSUPPORTED=7 } ScResult;
typedef enum ScType { SC_BOOL=1, SC_INT=2, SC_FLOAT=3, SC_ENUM=4,
    SC_STRING=5, SC_KEY=6, SC_BUTTON=7, SC_INFO=8,
    SC_HEADER=9, SC_DIVIDER=10, SC_DESCRIPTION=11,
    SC_COLOR=12 /* packed 0xRRGGBB in ScValue.integer */ } ScType;
typedef enum ScFlags { SC_RELOAD_MOD=1u, SC_RESTART_GAME=2u,
    SC_READ_ONLY=4u } ScFlags;
/* Both modes save registered values to SleekConfig/<mod-id>.dat. Per setting,
   saved DAT wins over the mod's INI, which wins over the registered default.
   SC_EXTERNAL may mirror a mod-owned file through on_save and publish its
   loaded values; SC_MANAGED_INI imports a legacy INI. */
typedef enum ScPersistence { SC_EXTERNAL=0, SC_MANAGED_INI=1 } ScPersistence;
typedef struct ScValue {
    uint32_t type; /* ScType */
    int32_t integer; /* bool, int, enum, virtual key, RGB color */
    float real;
    char text[SC_TEXT_CAP]; /* UTF-8, always NUL terminated */
} ScValue;
typedef int (SC_CALL *ScChangeFn)(void* user, const char* setting_id,
    const ScValue* proposed); /* 0 rejects; game main thread */
typedef int (SC_CALL *ScPersistenceFn)(void* user); /* 0 indicates failure */
typedef struct ScModDesc {
    uint32_t size;
    uint32_t required_api;
    const char* id;
    const char* name;
    const char* author;
    const char* description;
    const char* version;
    const char* category;
    const char* icon_path; /* optional PNG path relative to GTA SA directory */
    const char* dependencies; /* informational comma-separated stable IDs */
    uint32_t persistence; /* ScPersistence */
    const char* ini_path; /* managed mode legacy import path, under game directory */
    ScChangeFn on_change;
    ScPersistenceFn on_load;
    ScPersistenceFn on_save;
    void* user;
} ScModDesc;
typedef struct ScSettingDesc {
    uint32_t size;
    const char* id;
    const char* label;
    const char* page;
    const char* group; /* slash-delimited nested group path */
    const char* description;
    const char* tooltip;
    uint32_t type; /* ScType */
    uint32_t flags; /* ScFlags */
    ScValue default_value;
    double minimum;
    double maximum;
    double step;
    const char* options; /* enum labels separated by | */
} ScSettingDesc;
typedef struct ScApi {
    uint32_t size;
    uint32_t version;
    uint32_t capabilities; /* SC_CAP_* */
    ScResult (SC_CALL *register_mod)(const ScModDesc*, ScMod*);
    ScResult (SC_CALL *unregister_mod)(ScMod);
    ScResult (SC_CALL *register_setting)(ScMod, const ScSettingDesc*);
    ScResult (SC_CALL *get_value)(ScMod, const char*, ScValue*);
    ScResult (SC_CALL *set_value)(ScMod, const char*, const ScValue*);
    ScResult (SC_CALL *save)(ScMod);
    ScResult (SC_CALL *reload)(ScMod);
    ScResult (SC_CALL *reset)(ScMod);
    uint64_t (SC_CALL *revision)(ScMod);
    const char* (SC_CALL *last_error)(ScMod); /* valid until next API call */
    ScResult (SC_CALL *publish_value)(ScMod, const char*, const ScValue*);
    /* external backend: update the displayed value after reading its own file,
       without invoking on_change or marking the setting dirty */
} ScApi;
typedef const ScApi* (SC_CALL *ScGetApiFn)(uint32_t version, uint32_t size);
/* SleekConfig_GetApi is resolved by name with GetProcAddress. */
/* Optional export. Read a saved DAT value before game initialization; callers
   must verify type and bounds. SC_NOT_FOUND means no saved override. */
typedef ScResult (SC_CALL *ScReadStoredFn)(const char* mod_id,
    const char* setting_id, ScValue* value);

/* Optional extension: the v1 ScApi and existing descriptors stay unchanged.
   Resolve SleekConfig_RegisterBoundSetting only when the capability is present.
   The ID is used at registration/persistence time; gameplay reads the variable
   directly. Values flow from SleekConfig to the variable, not in reverse. */
typedef enum ScStorage { SC_STORAGE_BOOL8=1, SC_STORAGE_INT32=2,
    SC_STORAGE_FLOAT=3, SC_STORAGE_TEXT=4 } ScStorage;
typedef struct ScBindingDesc {
    uint32_t size;
    uint32_t storage; /* ScStorage */
    void* variable;
    uint32_t capacity; /* bytes: 1, 4, 4, or >= SC_TEXT_CAP respectively */
} ScBindingDesc;
/* BOOL8: SC_BOOL; INT32: bool/int/enum/key/color; FLOAT: SC_FLOAT;
   TEXT: SC_STRING. Registration writes the committed default immediately.
   Accepted edits, publish, startup load, reload and reset update the variable.
   on_change remains optional and can reject before the variable is written.
   All calls/writes are on the game main thread. Keep writable storage alive
   and at a stable address until unregister; unregister before DLL unload.
   No variable is registered or written if registration fails. */
typedef ScResult (SC_CALL *ScRegisterBoundSettingFn)(ScMod mod,
    const ScSettingDesc* setting, const ScBindingDesc* binding);

#ifdef __cplusplus
}
#endif
#endif
