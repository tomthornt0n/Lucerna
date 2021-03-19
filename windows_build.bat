@echo off

PUSHD bin

CALL ..\platform\windows_build.bat %1
CALL ..\meta\windows_build.bat

call .\meta.exe

SET debug_compiler_flags=/Od /Zi /DLUCERNA_DEBUG
SET debug_linker_flags=/DEBUG

SET release_compiler_flags=/O2 /arch:AVX
SET release_linker_flags= 

SET sources=..\game\main.c

SET common_compiler_flags=/nologo /utf-8 /I..\meta /I..\include /I..\game

SET common_linker_flags=/subsystem:windows /INCREMENTAL:NO /nologo platform_windows.lib /dll /EXPORT:game_init /EXPORT:game_update_and_render /EXPORT:game_audio_callback /EXPORT:game_cleanup /out:lucerna.dll


SET compiler_flags=%release_compiler_flags%
IF "%1"=="debug" SET compiler_flags=%debug_compiler_flags%

SET linker_flags=%release_linker_flags%
IF "%1"=="debug" SET linker_flags=%debug_linker_flags%

cl %common_compiler_flags% %compiler_flags% %sources% /link %common_linker_flags% %linker_flags%

DEL *.obj

POPD
