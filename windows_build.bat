@echo off

CD platform
CALL .\windows_build.bat
CD ..

CD meta
CALL .\windows_build.bat
CD ..

CD bin
COPY /a /b ..\game\*.lcd _tmp.lcd
.\lcddl .\lcddl_user_layer.dll _tmp.lcd
CD ..

cl /nologo /Imeta /Iinclude /DLUCERNA_DEBUG /Zi game\main.c /link /subsystem:console /nologo bin\platform_windows.lib /dll /EXPORT:game_init /EXPORT:game_update_and_render /EXPORT:game_audio_callback /EXPORT:game_cleanup /out:bin\lucerna.dll

del *.obj
del *.pdb