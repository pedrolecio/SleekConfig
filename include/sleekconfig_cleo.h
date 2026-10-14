#ifndef SLEEKCONFIG_CLEO_H
#define SLEEKCONFIG_CLEO_H
#include "sleekconfig.h"
#ifdef SLEEKCONFIG_BUILD
#define SC_CLEO_EXPORT __declspec(dllexport)
#else
#define SC_CLEO_EXPORT
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Optional script-facing exports, resolved with CLEO 0AA4. All return 0 for
   success except Register/Revision/GetInt. Register returns 0 on failure. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_Register(const char* id,const char* name);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_RegisterManaged(const char* id,const char* name,const char* ini_path);
/* Metadata-aware variants remain ABI compatible; CLEO 5 0AA7 cannot pass
   their six/seven inline string arguments. Use Register + SetMetadata there. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_RegisterEx(const char* id,const char* name,
    const char* author,const char* description,const char* version,const char* category);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_RegisterManagedEx(const char* id,const char* name,
    const char* author,const char* description,const char* version,const char* category,
    const char* ini_path);
/* Use after Register/ RegisterManaged from CLEO: four strings fit 0AA7's limit. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_SetMetadata(int handle,const char* author,
    const char* description,const char* version,const char* category);
/* Optional PNG icon path relative to the GTA SA directory. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_SetIcon(int handle,const char* icon_path);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_AddBool(int handle,const char* id,const char* label,int initial);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_AddInt(int handle,const char* id,const char* label,int initial,int minimum,int maximum);
/* Page-aware variants for tabs; missing page defaults to General. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_AddBoolPage(int handle,const char* id,
    const char* label,const char* page,int initial);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_AddIntPage(int handle,const char* id,
    const char* label,const char* page,int initial,int minimum,int maximum);
/* Options are pipe-separated labels; values are zero-based indexes. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_AddEnumPage(int handle,const char* id,
    const char* label,const char* page,const char* options,int initial);
/* RGB is packed as 0xRRGGBB; use GetInt to read it back. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_AddColorPage(int handle,const char* id,
    const char* label,const char* page,int initial_rgb);
/* For scripts using integer-only 0AA7 calls: real value = scaled integer / scale. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_AddScaledFloatPage(int handle,
    const char* id,const char* label,const char* page,
    int initial_scaled,int minimum_scaled,int maximum_scaled,int scale);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_GetScaledFloat(int handle,
    const char* id,int scale);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_GetInt(int handle,const char* id);
/* Read a saved DAT integer before registration; fallback if absent or invalid. */
SC_CLEO_EXPORT int SC_CALL SC_Cleo_ReadSavedInt(const char* mod_id,
    const char* setting_id,int fallback);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_SetInt(int handle,const char* id,int value);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_Revision(int handle);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_Save(int handle);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_Reload(int handle);
SC_CLEO_EXPORT int SC_CALL SC_Cleo_Unregister(int handle);
#ifdef __cplusplus
}
#endif
#endif
