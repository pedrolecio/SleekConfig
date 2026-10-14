SCRIPT_START
{
    // GTA3script syntax. Uses only standard CLEO commands.
    LVAR_INT library handle proc getInt getFloat revision unregister
    LVAR_INT lastRevision currentRevision status enabled strengthScaled tint
    LVAR_FLOAT strength

    WAIT 0
    library = 0
    handle = 0
    lastRevision = -1
    enabled = 1
    strengthScaled = 100
    strength = 1.0
    tint = 0x3399FF

    IF NOT LOAD_DYNAMIC_LIBRARY "SleekConfig.asi" (library)
        GOTO main_loop
    ENDIF
    // Resolve every required function before registering the mod.
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_Unregister" library (unregister)
        GOTO bridge_failed
    ENDIF
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_GetInt" library (getInt)
        GOTO bridge_failed
    ENDIF
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_GetScaledFloat" library (getFloat)
        GOTO bridge_failed
    ENDIF
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_Revision" library (revision)
        GOTO bridge_failed
    ENDIF
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_SetMetadata" library (proc)
        GOTO bridge_failed
    ENDIF
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_AddBoolPage" library (proc)
        GOTO bridge_failed
    ENDIF
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_AddScaledFloatPage" library (proc)
        GOTO bridge_failed
    ENDIF
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_AddColorPage" library (proc)
        GOTO bridge_failed
    ENDIF
    IF NOT GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_Register" library (proc)
        GOTO bridge_failed
    ENDIF
    CALL_FUNCTION_RETURN proc 2 2 ("My CLEO Effect" "example.cleo.effect")(handle)
    IF handle = 0
        GOTO bridge_failed
    ENDIF

    GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_SetMetadata" library (proc)
    CALL_FUNCTION_RETURN proc 5 5 ("Visuals" "1.0" "Live effect settings" "Your name" handle)(status)
    IF NOT status = 0
        GOTO bridge_failed
    ENDIF
    GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_AddBoolPage" library (proc)
    CALL_FUNCTION_RETURN proc 5 5 (enabled "General" "Enable effect" "enabled" handle)(status)
    IF NOT status = 0
        GOTO bridge_failed
    ENDIF
    GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_AddScaledFloatPage" library (proc)
    CALL_FUNCTION_RETURN proc 8 8 (100 300 0 strengthScaled "General" "Effect strength" "strength" handle)(status)
    IF NOT status = 0
        GOTO bridge_failed
    ENDIF
    GET_DYNAMIC_LIBRARY_PROCEDURE "SC_Cleo_AddColorPage" library (proc)
    CALL_FUNCTION_RETURN proc 5 5 (tint "Appearance" "Effect color" "tint" handle)(status)
    IF NOT status = 0
        GOTO bridge_failed
    ENDIF
    GOTO main_loop

bridge_failed:
    GOSUB detach

main_loop:
    WHILE TRUE
        WAIT 0
        IF handle > 0
            CALL_FUNCTION_RETURN revision 1 1 (handle)(currentRevision)
            IF NOT currentRevision = lastRevision
                lastRevision = currentRevision
                CALL_FUNCTION_RETURN getInt 2 2 ("enabled" handle)(enabled)
                CALL_FUNCTION_RETURN getInt 2 2 ("tint" handle)(tint)
                CALL_FUNCTION_RETURN getFloat 3 3 (100 "strength" handle)(strengthScaled)
                strength =# strengthScaled
                strength /= 100.0
            ENDIF
        ENDIF

        // Put your existing effect logic here. It must read enabled,
        // strength and tint for menu edits to affect the actual mod.
        // Without SleekConfig, the defaults above remain usable.
        IF IS_KEY_PRESSED 0x77 // F8 exits this demonstration.
            GOTO stop_demo
        ENDIF
    ENDWHILE
stop_demo:
    GOSUB detach
    TERMINATE_THIS_CUSTOM_SCRIPT

detach:
    IF handle > 0
        CALL_FUNCTION_RETURN unregister 1 1 (handle)(status)
        handle = 0
    ENDIF
    IF NOT library = 0
        FREE_DYNAMIC_LIBRARY library
        library = 0
    ENDIF
    RETURN
}
SCRIPT_END
