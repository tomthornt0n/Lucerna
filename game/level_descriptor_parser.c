typedef struct
{
 F32 player_spawn_x, player_spawn_y;
 S8 bg_path;
 S8 fg_path;
 S8 music_path;
 S8 entities_path;
} LevelDescriptor;

typedef enum
{
 LEVEL_DESCRIPTOR_TOKEN_KIND_error,
 LEVEL_DESCRIPTOR_TOKEN_KIND_alphanumeric_block,
 LEVEL_DESCRIPTOR_TOKEN_KIND_numeric_block,
 LEVEL_DESCRIPTOR_TOKEN_KIND_symbol,
 LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal,
} LevelDescriptorTokenKind;

typedef struct
{
 LevelDescriptorTokenKind kind;
 U8 *string;
 U64 string_length;
} LevelDescriptorToken;

internal LevelDescriptorToken
get_next_level_descriptor_token(U8 **read_pointer)
{
 while ((*read_pointer)[0] == ' '  ||
        (*read_pointer)[0] == '\t' ||
        (*read_pointer)[0] == '\n' ||
        (*read_pointer)[0] == '\r')
 {
  *read_pointer += 1;
 }
 
 LevelDescriptorToken result = {0};
 
 if (isdigit((*read_pointer)[0]) ||
     (*read_pointer)[0] == '-')
  // NOTE(tbt): numeric block
 {
  result.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_numeric_block;
  result.string = *read_pointer;
  result.string_length = 1;
  *read_pointer += 1;
  
  while (isdigit((*read_pointer)[0]) ||
         (*read_pointer)[0] == '.')
  {
   result.string_length += 1;
   *read_pointer += 1;
  }
 }
 else if (isalpha((*read_pointer)[0]) ||
          (*read_pointer)[0] == '_')
  // NOTE(tbt): alphanumeric block
 {
  result.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_alphanumeric_block;
  result.string = *read_pointer;
  result.string_length = 1;
  *read_pointer += 1;
  
  while (isalnum((*read_pointer)[0]) ||
         (*read_pointer)[0] == '_')
  {
   result.string_length += 1;
   *read_pointer += 1;
  }
 }
 else if ((*read_pointer)[0] == '"')
  // NOTE(tbt): string literals
 {
  *read_pointer += 1; // NOTE(tbt): eat opening "
  
  result.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal;
  result.string = *read_pointer;
  result.string_length = 1;
  *read_pointer += 1;
  
  while ((*read_pointer)[0] != '"')
  {
   result.string_length += 1;
   *read_pointer += 1;
  }
  
  *read_pointer += 1; // NOTE(tbt): eat closing "
 }
 else if ((*read_pointer)[0] == ',' ||
          (*read_pointer)[0] == ':')
  // NOTE(tbt): symbol
 {
  result.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol;
  result.string = *read_pointer;
  result.string_length = 1;
  *read_pointer += 1;
 }
 else
 {
  fprintf(stderr,
          "Error parsing level descriptor: unexpected character '%c'\n",
          (*read_pointer)[0]);
  *read_pointer += 1;
 }
 
 return result;
}

internal LevelDescriptor
parse_level_descriptor(S8 file)
{
 LevelDescriptor result = {0};
 
 U8 *buffer = file.buffer;
 while (buffer - file.buffer < file.len)
 {
  LevelDescriptorToken token = get_next_level_descriptor_token(&buffer);
  
  if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_alphanumeric_block)
  {
   if (0 == strncmp(token.string, "fg", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer);
     
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal)
     {
      S8 foreground_path = 
      {
       .buffer = token.string,
       .len = token.string_length,
      };
      
      result.fg_path = foreground_path;
     }
     else
     {
      fprintf(stderr, "Error parsing level descriptor: expected a string literal\n");
     }
    }
    else
    {
     fprintf(stderr, "Error parsing level descriptor: expected a colon\n");
    }
   }
   else if (0 == strncmp(token.string, "bg", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer);
     
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal)
     {
      S8 background_path = 
      {
       .buffer = token.string,
       .len = token.string_length,
      };
      
      result.bg_path = background_path;
     }
     else
     {
      fprintf(stderr, "Error parsing level descriptor: expected a string literal\n");
     }
    }
    else
    {
     fprintf(stderr, "Error parsing level descriptor: expected a colon\n");
    }
   }
   else if (0 == strncmp(token.string, "music", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer);
     
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal)
     {
      S8 music_path = 
      {
       .buffer = token.string,
       .len = token.string_length,
      };
      
      result.music_path = music_path;
     }
     else
     {
      fprintf(stderr, "Error parsing level descriptor: expected a string literal\n");
     }
    }
    else
    {
     fprintf(stderr, "Error parsing level descriptor: expected a colon\n");
    }
   }
   else if (0 == strncmp(token.string, "entities", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer);
     
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal)
     {
      S8 entities_source_path = 
      {
       .buffer = token.string,
       .len = token.string_length,
      };
      
      result.entities_path = entities_source_path;
     }
     else
     {
      fprintf(stderr, "Error parsing level descriptor: expected a string literal\n");
     }
    }
    else
    {
     fprintf(stderr, "Error parsing level descriptor: expected a colon\n");
    }
   }
   else if (0 == strncmp(token.string, "player_spawn", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer);
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_numeric_block)
     {
      char num_cstr_buf[512] = {0};
      
      snprintf(num_cstr_buf, 512, "%.*s", (I32)token.string_length, token.string);
      result.player_spawn_x = atof(num_cstr_buf);
      
      token = get_next_level_descriptor_token(&buffer);
      if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
          token.string[0] == ',')
      {
       token = get_next_level_descriptor_token(&buffer);
       if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_numeric_block)
       {
        snprintf(num_cstr_buf, 512, "%.*s", (I32)token.string_length, token.string);
        result.player_spawn_y = atof(num_cstr_buf);
       }
       else
       {
        fprintf(stderr, "Error parsing level descriptor: expected a numeric block\n");
       }
      }
      else
      {
       fprintf(stderr, "Error parsing level descriptor: expected a comma\n");
      }
     }
     else
     {
      fprintf(stderr, "Error parsing level descriptor: expected a numeric block\n");
     }
    }
    else
    {
     fprintf(stderr, "Error parsing level descriptor: expected a colon\n");
    }
   }
  }
  else
  {
   fprintf(stderr, "Error parsing level descriptor: expected an alphanumeric block\n");
  }
 }
 
 return result;
}