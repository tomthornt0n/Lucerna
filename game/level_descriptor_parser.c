// NOTE(tbt): forgive me for I have sinned
// NOTE(tbt): probably fine because it only has to handle files I write, but be wary of malformed input

typedef struct
{
 F32 player_spawn_x, player_spawn_y;
 S8 bg_path;
 S8 fg_path;
 S8 music_path;
 S8 entities_path;
 F32 exposure;
 B32 is_memory;
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
get_next_level_descriptor_token(U8 **read_pointer,
                                U64 *length)
{
 while (*length > 0 &&
        (*read_pointer)[0] == ' '  ||
        (*read_pointer)[0] == '\t' ||
        (*read_pointer)[0] == '\n' ||
        (*read_pointer)[0] == '\r')
 {
  *read_pointer += 1;
  *length -= 1;
 }
 
 LevelDescriptorToken result = {0};
 
 if (*length > 0)
 {
  if (isdigit((*read_pointer)[0]) ||
      (*read_pointer)[0] == '-')
   // NOTE(tbt): numeric block
  {
   result.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_numeric_block;
   result.string = *read_pointer;
   result.string_length = 1;
   *read_pointer += 1;
   *length -= 1;
   
   while (*length > 0 &&
          isdigit((*read_pointer)[0]) ||
          (*read_pointer)[0] == '.')
   {
    result.string_length += 1;
    *read_pointer += 1;
    *length -= 1;
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
   *length -= 1;
   
   while (*length > 0 &&
          isalnum((*read_pointer)[0]) ||
          (*read_pointer)[0] == '_')
   {
    result.string_length += 1;
    *read_pointer += 1;
    *length -= 1;
   }
  }
  else if ((*read_pointer)[0] == '"')
   // NOTE(tbt): string literals
  {
   *read_pointer += 1; // NOTE(tbt): eat opening "
   *length -= 1;
   
   // NOTE(tbt): will break with empty strings
   result.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal;
   result.string = *read_pointer;
   result.string_length = 1;
   *read_pointer += 1;
   *length -= 1;
   
   while (*length > 0 &&
          (*read_pointer)[0] != '"')
   {
    result.string_length += 1;
    *read_pointer += 1;
    *length -= 1;
   }
   
   if (length > 0 &&
       *read_pointer[0] == '"')
   {
    *read_pointer += 1; // NOTE(tbt): eat closing "
    *length -= 1;
   }
   else
   {
    fprintf(stderr, "Error parsing level descriptor: expected closing '\"'\n");
    result = (LevelDescriptorToken){0};
    goto end;
   }
  }
  else if ((*read_pointer)[0] == ',' ||
           (*read_pointer)[0] == ':')
   // NOTE(tbt): symbol
  {
   result.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol;
   result.string = *read_pointer;
   result.string_length = 1;
   *read_pointer += 1;
   *length -= 1;
  }
  else
  {
   fprintf(stderr,
           "    Error parsing level descriptor: unexpected character '%c'\n",
           (*read_pointer)[0]);
   *read_pointer += 1;
   *length -= 1;
  }
 }
 
 end:
 return result;
}

internal LevelDescriptor
parse_level_descriptor(S8 file)
{
 LevelDescriptor result = {0};
 
 U8 *buffer = file.buffer;
 U64 chars_remaining = file.len;
 while (chars_remaining)
 {
  LevelDescriptorToken token = get_next_level_descriptor_token(&buffer, &chars_remaining);
  
  if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_alphanumeric_block)
  {
   if (0 == strncmp(token.string, "fg", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer, &chars_remaining);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer, &chars_remaining);
     
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal)
     {
      S8 foreground_path = 
      {
       .buffer = token.string,
       .len = token.string_length,
      };
      
      result.fg_path = foreground_path;
      fprintf(stderr, "    Foreground path = %.*s\n", (I32)foreground_path.len, foreground_path.buffer);
     }
     else
     {
      fprintf(stderr, "    Error parsing level descriptor: expected a string literal as value for field 'fg'\n");
     }
    }
    else
    {
     fprintf(stderr, "    Error parsing level descriptor: expected a colon after 'fg'\n");
    }
   }
   else if (0 == strncmp(token.string, "bg", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer, &chars_remaining);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer, &chars_remaining);
     
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal)
     {
      S8 background_path = 
      {
       .buffer = token.string,
       .len = token.string_length,
      };
      
      result.bg_path = background_path;
      fprintf(stderr, "    Background path = %.*s\n", (I32)background_path.len, background_path.buffer);
     }
     else
     {
      fprintf(stderr, "    Error parsing level descriptor: expected a string literal as value for field 'fg'\n");
     }
    }
    else
    {
     fprintf(stderr, "    Error parsing level descriptor: expected a colon after 'bg'\n");
    }
   }
   else if (0 == strncmp(token.string, "music", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer, &chars_remaining);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer, &chars_remaining);
     
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal)
     {
      S8 music_path = 
      {
       .buffer = token.string,
       .len = token.string_length,
      };
      
      result.music_path = music_path;
      fprintf(stderr, "    Music path = %.*s\n", (I32)result.music_path.len, result.music_path.buffer);
     }
     else
     {
      fprintf(stderr, "    Error parsing level descriptor: expected a string literal as value for field 'music'\n");
     }
    }
    else
    {
     fprintf(stderr, "    Error parsing level descriptor: expected a colon after 'music'\n");
    }
   }
   else if (0 == strncmp(token.string, "entities", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer, &chars_remaining);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer, &chars_remaining);
     
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_string_literal)
     {
      S8 entities_source_path = 
      {
       .buffer = token.string,
       .len = token.string_length,
      };
      
      result.entities_path = entities_source_path;
      fprintf(stderr, "    Entities path = %.*s\n", (I32)result.entities_path.len, result.entities_path.buffer);
     }
     else
     {
      fprintf(stderr, "    Error parsing level descriptor: expected a string literal as value for field 'entities'\n");
     }
    }
    else
    {
     fprintf(stderr, "    Error parsing level descriptor: expected a colon after 'entities'\n");
    }
   }
   else if (0 == strncmp(token.string, "player_spawn", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer, &chars_remaining);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer, &chars_remaining);
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_numeric_block)
     {
      char num_cstr_buf[512] = {0};
      
      snprintf(num_cstr_buf, 512, "%.*s", (I32)token.string_length, token.string);
      result.player_spawn_x = atof(num_cstr_buf);
      
      token = get_next_level_descriptor_token(&buffer, &chars_remaining);
      if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
          token.string[0] == ',')
      {
       token = get_next_level_descriptor_token(&buffer, &chars_remaining);
       if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_numeric_block)
       {
        snprintf(num_cstr_buf, 512, "%.*s", (I32)token.string_length, token.string);
        result.player_spawn_y = atof(num_cstr_buf);
        
        fprintf(stderr, "    Player spawn = (%f, %f)\n", result.player_spawn_x, result.player_spawn_y);
       }
       else
       {
        fprintf(stderr, "    Error parsing level descriptor: expected a numeric block as value for 'player_spawn_y'\n");
       }
      }
      else
      {
       fprintf(stderr, "    Error parsing level descriptor: expected a comma separating 'player_spawn_x' and 'player_spawn_y'\n");
      }
     }
     else
     {
      fprintf(stderr, "    Error parsing level descriptor: expected a numeric block as value for 'player_spawn_x'\n");
     }
    }
    else
    {
     fprintf(stderr, "    Error parsing level descriptor: expected a colon after 'player_spawn'\n");
    }
   }
   else if (0 == strncmp(token.string, "exposure", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer, &chars_remaining);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer, &chars_remaining);
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_numeric_block)
     {
      char num_cstr_buf[512] = {0};
      
      snprintf(num_cstr_buf, 512, "%.*s", (I32)token.string_length, token.string);
      result.exposure = atof(num_cstr_buf);
      fprintf(stderr, "    Exposure = %f\n", result.exposure);
     }
     else
     {
      fprintf(stderr, "    Error parsing level descriptor: expected a numeric block as value for field 'exposure'\n");
     }
    }
    else
    {
     fprintf(stderr, "    Error parsing level descriptor: expected a colon after 'exposure'\n");
    }
   }
   else if (0 == strncmp(token.string, "kind", token.string_length))
   {
    token = get_next_level_descriptor_token(&buffer, &chars_remaining);
    if (token.kind = LEVEL_DESCRIPTOR_TOKEN_KIND_symbol &&
        token.string[0] == ':')
    {
     token = get_next_level_descriptor_token(&buffer, &chars_remaining);
     if (token.kind == LEVEL_DESCRIPTOR_TOKEN_KIND_alphanumeric_block)
     {
      if (0 == strncmp(token.string, "world", token.string_length))
      {
       result.is_memory = false;
       fprintf(stderr, "    Kind = world\n");
      }
      else if (0 == strncmp(token.string, "memory", token.string_length))
      {
       result.is_memory = true;
       fprintf(stderr, "    Kind = memory\n");
      }
      else
      {
       fprintf(stderr, "    Error parsing level descriptor: expected 'world' or 'memory' for field 'kind', got '%.*s'\n", (I32)token.string_length, token.string);
      }
     }
     else
     {
      fprintf(stderr, "    Error parsing level descriptor: expected a alphanumeric block as value for field 'kind'\n");
     }
    }
    else
    {
     fprintf(stderr, "    Error parsing level descriptor: expected a colon after 'kind'\n");
    }
   }
  }
  else
  {
   fprintf(stderr, "    Error parsing level descriptor: expected an alphanumeric block as field identifier, instead got '%.*s' (%d)\n", (I32)token.string_length, token.string, token.kind);
  }
 }
 
 return result;
}