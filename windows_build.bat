@echo off

REM todo: build modes: debug/release, full/fast

CD platform
CALL .\windows_build.bat
CD ..

CD meta
CALL .\windows_build.bat
CD ..

CD bin
REM todo: entire folder
.\lcddl .\lcddl_user_layer.dll ..\assets\entities\player.lcd
CD ..

cl /nologo /Iinclude /DLUCERNA_DEBUG /Zi game\main.c /link /nologo bin\platform_windows.lib /dll /EXPORT:game_init /EXPORT:game_update_and_render /EXPORT:game_audio_callback /EXPORT:game_cleanup /out:bin\lucerna.dll

del *.obj
del *.pdb