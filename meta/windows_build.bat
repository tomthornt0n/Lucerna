@echo off

cl /nologo lcddl.c /link /nologo /subsystem:console /export:lcddl_write_node_to_file_as_c_struct /export:lcddl_write_node_to_file_as_c_enum /export:lcddl_does_node_have_tag /export:lcddl_find_top_level_declaration /export:lcddl_find_all_top_level_declarations_with_tag /export:lcddl_evaluate_expression /export:lcddl_get_annotation_value /out:..\bin\lcddl.exe
cl /nologo lcddl_user_layer.c /link ..\bin\lcddl.lib /nologo /dll /out:..\bin\lcddl_user_layer.dll

del *.obj
