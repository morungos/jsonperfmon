#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <glib_compat.h>

#define fprintf(...)

#define MAX_LEN 8192
/******************************************************************************************************************
 * Glib compatibility
 *****************************************************************************************************************/

size_t safe_strlen(const char *str)
{
    const char * end = (const char *)memchr(str, '\0', MAX_LEN);
    if (end == NULL)
        return MAX_LEN;
    else
        return end - str;
}

void *g_new0(size_t  dfl_size)
{
  void *temp = malloc(dfl_size);
  memset(temp, 0, dfl_size);
  return temp;
}

GString* g_string_sized_new(size_t  dfl_size)
{
  GString *temp = (GString*)malloc(sizeof(GString));
  if (temp)
  {
    temp->len = 0;
    temp->allocated_len = dfl_size + 1 ;
    if ((temp->str = (char*)malloc(temp->allocated_len + 1)) != NULL)
      return temp;
    free(temp);
  }
  return NULL;
}

char* g_string_free(GString *string, int __attribute__((unused)) free_segment)
{
  free(string->str);
  free(string);
  return NULL;
}

void g_string_append_printf(GString *string, const char *format, ...)
{
  va_list vl;
  va_start(vl, format);

  size_t required = vsnprintf(string->str + string->len,
                              string->allocated_len - string->len,
                              format, vl);
  fprintf(stderr, "g_string_append_printf 1 str=%p, len=%lu, allocated_len=%lu, required=%lu\n", string->str, string->len, string->allocated_len, required );
  if ( (string->len + required) >= string->allocated_len)
  {
    size_t new_len  = string->len + required + 512; /* required includes null terminated char */
    char *tmp = (char*)malloc(new_len + 1);
    if (tmp) /* check "out of memory" */
    {
      memcpy(tmp, string->str, string->len+1);
      free(string->str);
      string->str = tmp;
      string->allocated_len = new_len;

      fprintf(stderr, "g_string_append_printf 2 str=%p, len=%lu, allocated_len=%lu, required=%lu\n", string->str, string->len, string->allocated_len, required );

      va_end(vl);
      va_start(vl, format);

      required = vsnprintf(string->str + string->len,
                                string->allocated_len - string->len,
                                format, vl);
      string->len += required;
    }
  }
  else
    string->len += required;

  fprintf(stderr, "g_string_append_printf 3 str=%p, len=%lu, allocated_len=%lu, required=%lu\n", string->str, string->len, string->allocated_len, required );
  fprintf(stderr, "Format -%s-\nString = -%.*s-\n", format, (int)string->len, string->str );
  va_end(vl);
}

GString* g_string_append(GString *string, const char *val)
{
  size_t required = safe_strlen(val);

  fprintf(stderr, "g_string_append        1 str=%p, len=%lu, allocated_len=%lu, required=%lu\n", string->str, string->len, string->allocated_len, required );
  if ( (string->len + required) >= string->allocated_len)
  {
    size_t new_len  = string->len + required + 512;
    char *tmp = (char*)malloc(new_len + 1);
    if (tmp) /* check "out of memory" */
    {
      memcpy(tmp, string->str, string->len+1);
      free(string->str);
      string->str = tmp;
      string->allocated_len = new_len;

      fprintf(stderr, "g_string_append        2 str=%p, len=%lu, allocated_len=%lu, required=%lu\n", string->str, string->len, string->allocated_len, required );
      memcpy( string->str + string->len, val, required + 1 );
      string->len += required;
    }
  }
  else
  {
    memcpy( string->str + string->len, val, required + 1 );
    string->len += required;
  }
  fprintf(stderr, "g_string_append        3 str=%p, len=%lu, allocated_len=%lu\n", string->str, string->len, string->allocated_len);

  return string;
}

GString* g_string_assign(GString *string, const char *val)
{
  size_t required = safe_strlen(val);

  if (required >= string->allocated_len)
  {
    size_t new_len  = required + 512;
    char *tmp = (char*)malloc(new_len + 1);
    if (tmp) /* check "out of memory" */
    {
      memcpy(tmp, string->str, string->len+1);
      free(string->str);
      string->str = tmp;
      string->allocated_len = new_len;

      memcpy( string->str, val, required + 1 );
      string->len = required;
    }
  }
  else
  {
    memcpy( string->str, val, required + 1 );
    string->len = required;
  }
  return string;
}
