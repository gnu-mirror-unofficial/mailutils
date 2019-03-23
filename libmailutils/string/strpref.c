#include <config.h>
#include <string.h>
#include <mailutils/cstr.h>

int
mu_string_prefix (char const *str, char const *pfx)
{
  size_t len = strlen (pfx);
  if (strlen (str) < len)
    return 0;
  return memcmp (str, pfx, len) == 0;
}
