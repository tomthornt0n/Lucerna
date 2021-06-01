@echo off

PUSHD bin
ECHO:
ECHO ~~~~~~~~~~~~~~~~~~~~ BUILDING PLATFORM LAYER ~~~~~~~~~~~~~~~~~~~~
SET platform_debug_compiler_flags=/Od /Zi /DLUCERNA_DEBUG
SET platform_debug_linker_flags=/DEBUG

SET platform_release_compiler_flags=/O2 /arch:AVX
SET platform_release_linker_flags= 

SET platform_common_compiler_flags=/nologo /I../include
SET platform_common_linker_flags=/stack:4194304 /INCREMENTAL:NO /subsystem:windows Dsound.lib User32.lib Gdi32.lib opengl32.lib /out:platform_windows.exe

SET platform_sources=..\source\lucerna_windows.c

SET platform_compiler_flags=%platform_release_compiler_flags%
IF "%1"=="debug" SET platform_compiler_flags=%platform_debug_compiler_flags%

SET platform_linker_flags=%platform_release_linker_flags%
IF "%1"=="debug" SET platform_linker_flags=%platform_debug_linker_flags%

CL %platform_common_compiler_flags% %platform_compiler_flags% %platform_sources% /link %platform_common_linker_flags% %platform_linker_flags% 

ECHO:
ECHO ~~~~~~~~~~~~~~~~~~~~~~~~~ BUILDING GAME ~~~~~~~~~~~~~~~~~~~~~~~~~
SET debug_compiler_flags=/Od /Zi /DLUCERNA_DEBUG
SET debug_linker_flags=/DEBUG

SET release_compiler_flags=/O2 /arch:AVX
SET release_linker_flags= 

SET sources=..\source\lucerna_game.c

SET common_compiler_flags=/nologo /utf-8 /I..\include

SET common_linker_flags=/subsystem:windows /INCREMENTAL:NO /nologo platform_windows.lib /dll /EXPORT:game_init /EXPORT:game_update_and_render /EXPORT:game_audio_callback /EXPORT:game_cleanup /out:lucerna.dll


SET compiler_flags=%release_compiler_flags%
IF "%1"=="debug" SET compiler_flags=%debug_compiler_flags%

SET linker_flags=%release_linker_flags%
IF "%1"=="debug" SET linker_flags=%debug_linker_flags%

CL %common_compiler_flags% %compiler_flags% %sources% /link %common_linker_flags% %linker_flags%

DEL *.obj

POPD
