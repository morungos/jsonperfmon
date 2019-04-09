/******************************************************************************************************************
 * Glib compatibility
 *****************************************************************************************************************/
#ifndef _COMPAT_GLIB_
#define _COMPAT_GLIB_

typedef struct _GString
{
  char  *str;
  size_t len;
  size_t allocated_len;
} GString;

size_t safe_strlen(const char*str);

void *g_new0(size_t  dfl_size);

GString* g_string_sized_new     (size_t  dfl_size);
void     g_string_append_printf (GString *string, const char *format, ...)  __attribute__((format(printf, 2, 0)));
GString* g_string_append        (GString *string, const char *val);
GString* g_string_assign        (GString *string, const char *val);
char*    g_string_free          (GString   *string, int free_segment);

#endif /* _COMPAT_GLIB_ */
