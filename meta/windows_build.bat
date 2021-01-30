@echo off

cl /nologo lcddl_user_layer.c /link /nologo /dll /out:..\bin\lcddl_user_layer.dll
cl /nologo lcddl.c /link /nologo /out:..\bin\lcddl.exe

del *.obj