#include <stdio.h>
#include <stdbool.h>
#include "lcddl.h"

internal void
_write_string_as_word_delimited_f(FILE *file,
                                  char *string,
                                  char delimiter)
{
 for (long i = 0;
      i < strlen(string);
      ++i)
 {
  if (string[i] == '_')
  {
   putc(delimiter, file);
  }
  else
  {
   if (string[i] >= 65 &&
       string[i] <= 90)
   {
    if (i) { putc(delimiter, file); }
    putc(string[i] + 32, file);
   }
   else
   {
    putc(string[i], file);
   }
  }
 }
}

internal void
write_string_as_lowercase_with_underscores_f(FILE *file,
                                             char *string)
{
 _write_string_as_word_delimited_f(file, string, '_');
}

internal void
write_string_as_lowercase_with_spaces_f(FILE *file,
                                        char *string)
{
 _write_string_as_word_delimited_f(file, string, ' ');
}

internal void
float_from_annotation(LcddlNode *node,
                      char *tag,
                      float *result)
{
 // NOTE(tbt): very basic - only kind of expression evaluation done is unary '-'
 float negate = 1.0f;
 
 LcddlNode *expr;
 
 if (NULL != (expr = lcddl_get_annotation_value(node, tag)))
 {
  if (expr->kind == LCDDL_NODE_KIND_unary_operator &&
      expr->unary_operator.kind == LCDDL_UN_OP_KIND_negative)
  {
   expr = expr->unary_operator.operand;
   negate = -1.0f;
  }
  
  if (expr->kind == LCDDL_NODE_KIND_float_literal)
  {
   *result = atof(expr->literal.value) * negate;
  }
  else if (expr->kind == LCDDL_NODE_KIND_integer_literal)
  {
   *result = atoi(expr->literal.value) * negate;
  }
 }
}

internal void
gen_editor_ui(LcddlNode *node,
              FILE *file)
{
 fprintf(file, "internal void\ndo_");
 write_string_as_lowercase_with_underscores_f(file, node->declaration.name);
 fprintf(file, "_editor_ui(PlatformState *input,\n%s *x)\n{\n", node->declaration.name);
 
 fprintf(file, "begin_window(input, s8_literal(\"gen %s %d\"), s8_literal(\"", node->declaration.name, rand());
 write_string_as_lowercase_with_spaces_f(file, node->declaration.name);
 fprintf(file, " editor\"), 32.0f, 32.0f, 600.0f);\ndo_line_break();\n");
 
 for (LcddlNode *child = node->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  if (!lcddl_does_node_have_tag(child, "no_ui"))
  {
   LcddlNode *dropdown_source_name;
   if (NULL != (dropdown_source_name = lcddl_get_annotation_value(child, "radio_dropdown_source")))
   {
    LcddlNode *dropdown_source = lcddl_find_top_level_declaration(dropdown_source_name->literal.value);
    if (dropdown_source)
    {
     fprintf(file, "begin_dropdown(input, s8_literal(\"gen %s %d\"), s8_literal(\"", child->declaration.name, rand());
     write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
     fprintf(file, "\"), 150.0f);\n");
     
     for (LcddlNode *menu_item = dropdown_source->first_child;
          NULL != menu_item;
          menu_item = menu_item->next_sibling)
     {
      if (!lcddl_does_node_have_tag(menu_item, "no_ui"))
      {
       fprintf(file,
               "if (do_button(input, s8_literal(\"gen %s %d\"), s8_literal(\"%s\"), 150.0f)) { x->%s = %s; }\n",
               menu_item->declaration.name,
               rand(),
               menu_item->declaration.name,
               child->declaration.name,
               menu_item->declaration.name);
      }
     }
     
     fprintf(file, "_ui_pop_insertion_point();\n");
     fprintf(file, "do_line_break();\n");
    }
   }
   else if (NULL != (dropdown_source_name = lcddl_get_annotation_value(child, "flag_dropdown_source")))
   {
    LcddlNode *dropdown_source = lcddl_find_top_level_declaration(dropdown_source_name->literal.value);
    if (dropdown_source)
    {
     fprintf(file, "begin_dropdown(input, s8_literal(\"gen %s %d\"), s8_literal(\"", child->declaration.name, rand());
     write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
     fprintf(file, "\"), 150.0f);\n");
     
     for (LcddlNode *menu_item = dropdown_source->first_child;
          NULL != menu_item;
          menu_item = menu_item->next_sibling)
     {
      if (!lcddl_does_node_have_tag(menu_item, "no_ui"))
      {
       fprintf(file,
               "do_bit_toggle_button(input, s8_literal(\"gen %s %d\"), s8_literal(\"%s\"), &x->%s, %s, 150.0f);\n",
               menu_item->declaration.name,
               rand(),
               menu_item->declaration.name,
               child->declaration.name,
               menu_item->declaration.name);
      }
     }
     
     fprintf(file, "_ui_pop_insertion_point();\n");
     fprintf(file, "do_line_break();\n");
    }
   }
   else if (0 == strcmp(child->declaration.type->type.type_name, "U8") &&
            0 != child->declaration.type->type.array_count)
   {
    fprintf(file,
            "do_label(s8_literal(\"gen %s label %d\"), s8_literal(\"",
            child->declaration.name,
            rand());
    write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
    fprintf(file, ":\"), 100.0f);\n", child->declaration.name);
    
    fprintf(file, "");
    fprintf(file,
            "do_text_entry(input, s8_literal(\"gen %s entry %d\"), x->%s, NULL, %u);\ndo_line_break();\n",
            child->declaration.name,
            rand(),
            child->declaration.name,
            child->declaration.type->type.array_count);
   }
   else if (0 == child->declaration.type->type.array_count &&
            0 == child->declaration.type->type.indirection_level)
   {
    if (0 == strcmp(child->declaration.type->type.type_name, "B32"))
    {
     fprintf(file,
             "do_toggle_button(input, s8_literal(\"gen %s %d\"), s8_literal(\"",
             child->declaration.name,
             rand());
     write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
     fprintf(file, "\"), 150.0f, &x->%s);\ndo_line_break();\n", child->declaration.name);
    }
    else
    {
     char slider_kind[3] = {0};
     
     if (0 == strcmp(child->declaration.type->type.type_name, "F64"))
     {
      slider_kind[0] = 'l';
      slider_kind[1] = 'f';
     }
     else if (0 == strcmp(child->declaration.type->type.type_name, "F32"))
     {
      slider_kind[0] = 'f';
     }
     else if (0 == strcmp(child->declaration.type->type.type_name, "I64"))
     {
      slider_kind[0] = 'l';
     }
     else if (0 == strcmp(child->declaration.type->type.type_name, "I32"))
     {
      slider_kind[0] = 'i';
     }
     
     if (slider_kind[0])
     {
      float min = 0.0f;
      float max = 100.0f;
      float snap = -1.0f;
      
      float_from_annotation(child, "min", &min);
      float_from_annotation(child, "max", &max);
      float_from_annotation(child, "snap", &snap);
      
      fprintf(file,
              "do_label(s8_literal(\"gen %s label %d\"), s8_literal(\"",
              child->declaration.name,
              rand());
      write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
      fprintf(file, ":\"), 100.0f);\n", child->declaration.name);
      fprintf(file,
              "do_slider_%s(input, s8_literal(\"gen %s slider %d\"), %ff, %ff, %ff, 200.0f, &x->%s);\ndo_line_break();\n",
              slider_kind,
              child->declaration.name,
              rand(),
              min,
              max,
              snap,
              child->declaration.name);
     }
    }
   }
  }
 }
 fprintf(file, "_ui_pop_insertion_point();\n");
 fprintf(file, "}\n\n");
}

// NOTE(tbt): doesn't recursively enter sub structs / unions / whatever or do anything fancy
internal void
gen_serialisation_funcs(LcddlNode *node,
                        FILE *file)
{
 //
 // NOTE(tbt): serialisation
 //
 
 {
  fprintf(file, "internal void\nserialise_");
  write_string_as_lowercase_with_underscores_f(file, node->declaration.name);
  fprintf(file,
          "(%s *x,\nPlatformFile *file)\n{\n",
          node->declaration.name);
  
  for (LcddlNode *child = node->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   if (!child->first_child &&
       child->kind == LCDDL_NODE_KIND_declaration &&
       child->declaration.type->type.indirection_level == 0)
   {
    if (0 == strcmp(child->declaration.type->type.type_name, "S8"))
     // NOTE(tbt): strings
    {
     fprintf(file,
             "platform_append_to_file(file, &(x->%s.len), sizeof(U64));\n"
             "platform_append_to_file(file, x->%s.buffer, x->%s.len);\n",
             child->declaration.name,
             child->declaration.name,
             child->declaration.name);
    }
    else if (0 == strcmp(child->declaration.type->type.type_name, "Rect"))
     // NOTE(tbt): rectangles
    {
     fprintf(file,
             "platform_append_to_file(file, &(x->%s.x), sizeof(x->%s.x));\n"
             "platform_append_to_file(file, &(x->%s.y), sizeof(x->%s.y));\n"
             "platform_append_to_file(file, &(x->%s.w), sizeof(x->%s.w));\n"
             "platform_append_to_file(file, &(x->%s.h), sizeof(x->%s.h));\n",
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name);
    }
    else if (0 == strcmp(child->declaration.type->type.type_name, "Colour"))
     // NOTE(tbt): colours
    {
     fprintf(file,
             "platform_append_to_file(file, &(x->%s.r), sizeof(x->%s.r));\n"
             "platform_append_to_file(file, &(x->%s.g), sizeof(x->%s.g));\n"
             "platform_append_to_file(file, &(x->%s.b), sizeof(x->%s.b));\n"
             "platform_append_to_file(file, &(x->%s.a), sizeof(x->%s.a));\n",
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name);
    }
    else if (0 == strcmp(child->declaration.type->type.type_name, "U8")  ||
             0 == strcmp(child->declaration.type->type.type_name, "U16") ||
             0 == strcmp(child->declaration.type->type.type_name, "U32") ||
             0 == strcmp(child->declaration.type->type.type_name, "U64") ||
             0 == strcmp(child->declaration.type->type.type_name, "I8")  ||
             0 == strcmp(child->declaration.type->type.type_name, "I16") ||
             0 == strcmp(child->declaration.type->type.type_name, "I32") ||
             0 == strcmp(child->declaration.type->type.type_name, "I64") ||
             0 == strcmp(child->declaration.type->type.type_name, "F32") ||
             0 == strcmp(child->declaration.type->type.type_name, "F64") ||
             0 == strcmp(child->declaration.type->type.type_name, "B32"))
     // NOTE(tbt): basic types
    {
     if (!child->declaration.type->type.array_count)
     {
      fprintf(file,
              "platform_append_to_file(file, &(x->%s), sizeof(x->%s));\n",
              child->declaration.name,
              child->declaration.name);
     }
     else
     {
      fprintf(file,
              "platform_append_to_file(file, x->%s, %u * sizeof(x->%s[0]));\n",
              child->declaration.name,
              child->declaration.type->type.array_count,
              child->declaration.name);
     }
    }
   }
  }
  fprintf(file, "}\n\n");
 }
 
 //
 // NOTE(tbt): deserialisation
 //
 
 {
  fprintf(file, "internal void\ndeserialise_");
  write_string_as_lowercase_with_underscores_f(file, node->declaration.name);
  fprintf(file, "(%s *x,\nU8 **read_pointer)\n{\n", node->declaration.name);
  
  for (LcddlNode *child = node->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   if (!child->first_child &&
       child->kind == LCDDL_NODE_KIND_declaration &&
       child->declaration.type->type.indirection_level == 0)
   {
    if (0 == strcmp(child->declaration.type->type.type_name, "S8"))
     // NOTE(tbt): strings
    {
     fprintf(file,
             "x->%s.len = *((U64 *)(*read_pointer));\n"
             "*read_pointer += sizeof(U64);\n"
             "if (x->%s.len)\n{\n"
             "x->%s.buffer = arena_allocate(&global_static_memory, x->%s.len);\n"
             "memcpy(x->%s.buffer, *read_pointer, x->%s.len);\n"
             "*read_pointer += x->%s.len;\n}\n"
             "else\n{\n"
             "x->%s.buffer = NULL;\n}\n",
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name);
    }
    else if (0 == strcmp(child->declaration.type->type.type_name, "Rect"))
     // NOTE(tbt): rectangles
    {
     fprintf(file,
             "x->%s.x = *((F32 *)(*read_pointer));\n"
             "*read_pointer += sizeof(F32);\n"
             "x->%s.y = *((F32 *)(*read_pointer));\n"
             "*read_pointer += sizeof(F32);\n"
             "x->%s.w = *((F32 *)(*read_pointer));\n"
             "*read_pointer += sizeof(F32);\n"
             "x->%s.h = *((F32 *)(*read_pointer));\n"
             "*read_pointer += sizeof(F32);\n",
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name);
    }
    else if (0 == strcmp(child->declaration.type->type.type_name, "Colour"))
     // NOTE(tbt): colours
    {
     fprintf(file,
             "x->%s.r = *((F32 *)(*read_pointer));\n"
             "x->%s.g = *((F32 *)(*read_pointer));\n"
             "x->%s.g = *((F32 *)(*read_pointer));\n"
             "x->%s.a = *((F32 *)(*read_pointer));\n"
             "*read_pointer += 4 * sizeof(F32);\n",
             child->declaration.name,
             child->declaration.name,
             child->declaration.name,
             child->declaration.name);
    }
    else if (0 == strcmp(child->declaration.type->type.type_name, "U8")  ||
             0 == strcmp(child->declaration.type->type.type_name, "U16") ||
             0 == strcmp(child->declaration.type->type.type_name, "U32") ||
             0 == strcmp(child->declaration.type->type.type_name, "U64") ||
             0 == strcmp(child->declaration.type->type.type_name, "I8")  ||
             0 == strcmp(child->declaration.type->type.type_name, "I16") ||
             0 == strcmp(child->declaration.type->type.type_name, "I32") ||
             0 == strcmp(child->declaration.type->type.type_name, "I64") ||
             0 == strcmp(child->declaration.type->type.type_name, "F32") ||
             0 == strcmp(child->declaration.type->type.type_name, "F64") ||
             0 == strcmp(child->declaration.type->type.type_name, "B32"))
     // NOTE(tbt): basic types
    {
     if (!child->declaration.type->type.array_count)
     {
      fprintf(file,
              "x->%s = *((%s *)(*read_pointer));\n"
              "*read_pointer += sizeof(x->%s);\n",
              child->declaration.name,
              child->declaration.type->type.type_name,
              child->declaration.name);
     }
     else
     {
      fprintf(file,
              "memcpy(x->%s, *read_pointer, %u * sizeof(x->%s[0]));\n"
              "*read_pointer += %u * sizeof(x->%s[0]);",
              child->declaration.name,
              child->declaration.type->type.array_count,
              child->declaration.name,
              child->declaration.type->type.array_count,
              child->declaration.name);
     }
    }
   }
  }
  fprintf(file, "}\n\n");
 }
}

LCDDL_CALLBACK void
lcddl_user_callback(LcddlNode *root)
{
 FILE *f = fopen("../include/types.gen.h", "wb");
 FILE *functions_file = fopen("../include/funcs.gen.h", "wb");
 
 for (LcddlNode *file = root->first_child;
      NULL != file;
      file = file->next_sibling)
 {
  for (LcddlNode *decl = file->first_child;
       NULL != decl;
       decl = decl->next_sibling)
  {
   if (lcddl_does_node_have_tag(decl, "c_struct"))
   {
    lcddl_write_node_to_file_as_c_struct(decl, f);
    
    if (lcddl_does_node_have_tag(decl, "gen_editor_ui"))
    {
     gen_editor_ui(decl, functions_file);
    }
    
    if (lcddl_does_node_have_tag(decl, "serialisable"))
    {
     gen_serialisation_funcs(decl, functions_file);
    }
   }
   else if (lcddl_does_node_have_tag(decl, "c_enum"))
   {
    lcddl_write_node_to_file_as_c_enum(decl, f);
   }
  }
 }
 
 fclose(f);
 fclose(functions_file);
}

