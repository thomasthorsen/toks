/**
 * @file logmask.cpp
 *
 * Functions to convert between a string and a severity mask.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "logmask.h"
#include <cstdio>      /* snprintf() */
#include <cstdlib>     /* strtoul() */
#include <cctype>


/**
 * Parses a string into a log severity
 *
 * @param str     The string to parse
 * @param mask    The mask to populate
 */
void logmask_from_string(const char *str, log_mask_t& mask)
{
   char *ptmp;
   bool was_dash   = false;
   int  last_level = -1;
   int  level;
   int  idx;

   if (str == NULL)
   {
      return;
   }

   /* Start with a clean mask */
   logmask_set_all(mask, false);

   /* If the first character is 'A', set all sevs */
   if (toupper((unsigned char) *str) == 'A')
   {
      logmask_set_all(mask, true);
      str++;
   }

   while (*str != 0)
   {
      if (isspace((unsigned char) *str))
      {
         str++;
         continue;
      }

      if (isdigit((unsigned char) *str))
      {
         level = strtoul(str, &ptmp, 10);
         str   = ptmp;

         logmask_set_sev(mask, (log_sev_t)level, true);
         if (was_dash)
         {
            for (idx = last_level + 1; idx < level; idx++)
            {
               logmask_set_sev(mask, (log_sev_t)idx, true);
            }
            was_dash = false;
         }

         last_level = level;
      }
      else if (*str == '-')
      {
         was_dash = true;
         str++;
      }
      else  /* probably a comma */
      {
         last_level = -1;
         was_dash   = false;
         str++;
      }
   }
}
