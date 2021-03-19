#include <stdio.h>
#include <stdbool.h>

#define LCDDL_AS_LIBRARY
#include "lcddl.c"


//
// NOTE(tbt): utilities
//~

static void
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

static void
write_string_as_lowercase_with_underscores_f(FILE *file,
                                             char *string)
{
 _write_string_as_word_delimited_f(file, string, '_');
}

static void
write_string_as_lowercase_with_spaces_f(FILE *file,
                                        char *string)
{
 _write_string_as_word_delimited_f(file, string, ' ');
}

static void
float_from_annotation(LcddlNode *node,
                      char *tag,
                      float *result)
{
 LcddlNode *expr;
 if (NULL != (expr = lcddl_get_annotation_value(node, tag)))
 {
  double value = lcddl_evaluate_expression(expr);
  *result = (float)value;
 }
}

static LcddlNode *
find_first_top_level_declaration(char *name)
{
 LcddlSearchResult *search_result = lcddl_find_top_level_declaration(name);
 if (search_result)
 {
  return search_result->node;
 }
 else
 {
  fprintf(stderr, "could not find top level declaration '%s'\n", name);
  return NULL;
 }
}

//
// NOTE(tbt): editor UI generation
//~

static void
gen_editor_ui(LcddlNode *node,
              FILE *file)
{
 fprintf(file, "internal void\ndo_");
 write_string_as_lowercase_with_underscores_f(file, node->declaration.name);
 fprintf(file, "_editor_ui(PlatformState *input,\n%s *x)\n{\n", node->declaration.name);
 
 for (LcddlNode *child = node->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  if (!lcddl_does_node_have_tag(child, "no_ui"))
  {
   char *line_break;
   if (lcddl_does_node_have_tag(child, "ui_horizontal_rule"))
   {
    line_break = "_ui_do_line_break(true)";
   }
   else
   {
    line_break = "_ui_do_line_break(false)";
   }
   LcddlNode *dropdown_source_name;
   if (NULL != (dropdown_source_name = lcddl_get_annotation_value(child, "radio_dropdown_source")))
   {
    LcddlNode *dropdown_source = find_first_top_level_declaration(dropdown_source_name->literal.value);
    if (dropdown_source)
    {
     fprintf(file, "_ui_begin_dropdown(input, s8_literal(\"gen %s %d\"), s8_literal(\"", child->declaration.name, rand());
     write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
     fprintf(file, "\"), -1.0f);\n");
     
     for (LcddlNode *menu_item = dropdown_source->first_child;
          NULL != menu_item;
          menu_item = menu_item->next_sibling)
     {
      if (!lcddl_does_node_have_tag(menu_item, "no_ui"))
      {
       fprintf(file,
               "if (ui_do_button(input, s8_literal(\"gen %s %d\"), s8_literal(\"%s\"), -1.0f)) { x->%s = %s; }\n",
               menu_item->declaration.name,
               rand(),
               menu_item->declaration.name,
               child->declaration.name,
               menu_item->declaration.name);
      }
     }
     
     fprintf(file, "_ui_pop_insertion_point();\n");
     fprintf(file, "%s;\n", line_break);
    }
   }
   else if (NULL != (dropdown_source_name = lcddl_get_annotation_value(child, "flag_dropdown_source")))
   {
    LcddlNode *dropdown_source = find_first_top_level_declaration(dropdown_source_name->literal.value);
    if (dropdown_source)
    {
     fprintf(file, "_ui_begin_dropdown(input, s8_literal(\"gen %s %d\"), s8_literal(\"", child->declaration.name, rand());
     write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
     fprintf(file, "\"), -1.0f);\n");
     
     for (LcddlNode *menu_item = dropdown_source->first_child;
          NULL != menu_item;
          menu_item = menu_item->next_sibling)
     {
      if (!lcddl_does_node_have_tag(menu_item, "no_ui"))
      {
       fprintf(file,
               "ui_do_bit_toggle_button(input, s8_literal(\"gen %s %d\"), s8_literal(\"%s\"), &x->%s, %s, -1.0f);\n",
               menu_item->declaration.name,
               rand(),
               menu_item->declaration.name,
               child->declaration.name,
               menu_item->declaration.name);
      }
     }
     
     fprintf(file, "_ui_pop_insertion_point();\n");
     fprintf(file, "%s;\n", line_break);
    }
   }
   else if (0 == strcmp(child->declaration.type->type.type_name, "U8") &&
            0 != child->declaration.type->type.array_count)
   {
    fprintf(file, "");
    fprintf(file,
            "ui_do_text_entry(input, s8_literal(\"gen %s entry %d\"), x->%s, NULL, %u);\n",
            child->declaration.name,
            rand(),
            child->declaration.name,
            child->declaration.type->type.array_count);
    
    fprintf(file,
            "ui_do_label(s8_literal(\"gen %s label %d\"), s8_literal(\"",
            child->declaration.name,
            rand());
    write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
    fprintf(file, ":\"), 100.0f);\n%s;\n", line_break);
   }
   else if (0 == child->declaration.type->type.array_count &&
            0 == child->declaration.type->type.indirection_level)
   {
    if (0 == strcmp(child->declaration.type->type.type_name, "B32"))
    {
     fprintf(file,
             "ui_do_toggle_button(input, s8_literal(\"gen %s %d\"), s8_literal(\"",
             child->declaration.name,
             rand());
     write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
     fprintf(file, "\"), -1.0f, &x->%s);\n%s;\n", child->declaration.name, line_break);
    }
    else
    {
     enum
     {
      SLIDER_KIND_none,
      SLIDER_KIND_double,
      SLIDER_KIND_float,
      SLIDER_KIND_long,
      SLIDER_KIND_int,
     };
     
     int slider_kind = 0;
     
     if (0 == strcmp(child->declaration.type->type.type_name, "F64"))
     {
      slider_kind = SLIDER_KIND_double;
     }
     else if (0 == strcmp(child->declaration.type->type.type_name, "F32"))
     {
      slider_kind = SLIDER_KIND_float;
     }
     else if (0 == strcmp(child->declaration.type->type.type_name, "I64"))
     {
      slider_kind = SLIDER_KIND_long;
     }
     else if (0 == strcmp(child->declaration.type->type.type_name, "I32"))
     {
      slider_kind = SLIDER_KIND_int;
     }
     
     if (slider_kind)
     {
      float min = 0.0f;
      float max = 100.0f;
      float snap = -1.0f;
      float width = 300.0f;
      
      float_from_annotation(child, "min", &min);
      float_from_annotation(child, "max", &max);
      float_from_annotation(child, "snap", &snap);
      float_from_annotation(child, "width", &width);
      
      fprintf(file,
              "ui_do_slider_%s(input, s8_literal(\"gen %s slider %d\"), %ff, %ff, %ff, %ff, &x->%s);\n",
              slider_kind == SLIDER_KIND_int    ? "i"  :
              slider_kind == SLIDER_KIND_long   ? "l"  :
              slider_kind == SLIDER_KIND_float  ? "f"  :
              slider_kind == SLIDER_KIND_double ? "lf" : NULL,
              child->declaration.name,
              rand(),
              min,
              max,
              snap,
              width,
              child->declaration.name);
      
      fprintf(file,
              "ui_do_label(s8_literal(\"gen %s label %d\"), s8_literal(\"",
              child->declaration.name,
              rand());
      write_string_as_lowercase_with_spaces_f(file, child->declaration.name);
      fprintf(file, ":\"), 100.0f);\n%s;\n", line_break);
     }
    }
   }
  }
 }
 fprintf(file, "}\n\n");
}

// NOTE(tbt): doesn't recursively enter sub structs / unions / whatever or do anything fancy
static void
gen_serialisation_funcs(LcddlNode *node,
                        FILE *file)
{
 //
 // NOTE(tbt): serialisation
 //~
 
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
             "platform_write_to_file_f(file, &(x->%s.len), sizeof(U64));\n"
             "platform_write_to_file_f(file, x->%s.buffer, x->%s.len);\n",
             child->declaration.name,
             child->declaration.name,
             child->declaration.name);
    }
    else if (0 == strcmp(child->declaration.type->type.type_name, "Rect"))
     // NOTE(tbt): rectangles
    {
     fprintf(file,
             "platform_write_to_file_f(file, &(x->%s.x), sizeof(x->%s.x));\n"
             "platform_write_to_file_f(file, &(x->%s.y), sizeof(x->%s.y));\n"
             "platform_write_to_file_f(file, &(x->%s.w), sizeof(x->%s.w));\n"
             "platform_write_to_file_f(file, &(x->%s.h), sizeof(x->%s.h));\n",
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
             "platform_write_to_file_f(file, &(x->%s.r), sizeof(x->%s.r));\n"
             "platform_write_to_file_f(file, &(x->%s.g), sizeof(x->%s.g));\n"
             "platform_write_to_file_f(file, &(x->%s.b), sizeof(x->%s.b));\n"
             "platform_write_to_file_f(file, &(x->%s.a), sizeof(x->%s.a));\n",
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
              "platform_write_to_file_f(file, &(x->%s), sizeof(x->%s));\n",
              child->declaration.name,
              child->declaration.name);
     }
     else
     {
      fprintf(file,
              "platform_write_to_file_f(file, x->%s, %u * sizeof(x->%s[0]));\n",
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
 //~
 
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
              "*read_pointer += %u * sizeof(x->%s[0]);\n",
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

//
// NOTE(tbt): main
//~

int
main(int argc,
     char **argv)
{
 FILE *f = fopen("../include/types.gen.h", "wb");
 FILE *functions_file = fopen("../include/funcs.gen.h", "wb");
 
 lcddl_initialise();
 
 LcddlNode *file = lcddl_parse_file("../game/lucerna.lcd");
 
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
 
 fclose(f);
 fclose(functions_file);
}

