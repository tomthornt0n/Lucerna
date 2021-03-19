SET platform_debug_compiler_flags=/Od /Zi /DLUCERNA_DEBUG
SET platform_debug_linker_flags=/DEBUG

SET platform_release_compiler_flags=/O2 /arch:AVX
SET platform_release_linker_flags= 

SET platform_common_compiler_flags=/nologo /I../include
SET platform_common_linker_flags=/stack:4194304 /INCREMENTAL:NO /subsystem:windows Dsound.lib User32.lib Gdi32.lib opengl32.lib /out:platform_windows.exe

SET platform_sources=..\platform\windows_lucerna.c

SET platform_compiler_flags=%platform_release_compiler_flags%
IF "%1"=="debug" SET platform_compiler_flags=%platform_debug_compiler_flags%

SET platform_linker_flags=%platform_release_linker_flags%
IF "%1"=="debug" SET platform_linker_flags=%platform_debug_linker_flags%

CL %platform_common_compiler_flags% %platform_compiler_flags% %platform_sources% /link %platform_common_linker_flags% %platform_linker_flags% 
