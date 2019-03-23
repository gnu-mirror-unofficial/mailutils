#include <config.h>
#include <string.h>
#include <mailutils/cstr.h>

int
mu_string_suffix (char const *str, char const *sfx)
{
  size_t sfxlen = strlen (sfx);
  size_t len = strlen (str); 
  if (len < sfxlen)
    return 0;
  return memcmp (str + len - sfxlen, sfx, sfxlen) == 0;
}
