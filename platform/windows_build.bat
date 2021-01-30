@echo off

cl /nologo /I../include /DLUCERNA_DEBUG windows_lucerna.c /Zi /Fd:..\bin\windows_lucerna.pdb /link /nologo /stack:4194304 /subsystem:windows Dsound.lib User32.lib Gdi32.lib opengl32.lib /out:..\bin\platform_windows.exe
DEL windows_lucerna.obj

