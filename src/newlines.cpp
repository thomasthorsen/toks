/**
 * @file newlines.cpp
 * Adds or removes newlines.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "toks_types.h"
#include "chunk_list.h"
#include "prototypes.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include "unc_ctype.h"


static void undo_one_liner(chunk_t *pc);

#define MARK_CHANGE()    mark_change(__func__, __LINE__)
static void mark_change(const char *func, int line)
{
   cpd.changes++;
   if (cpd.pass_count == 0)
   {
      LOG_FMT(LCHANGE, "%s: change %d on %s:%d\n", __func__, cpd.changes, func, line);
   }
}


/*
 * Basic approach:
 * 1. Find next open brace
 * 2. Find next close brace
 * 3. Determine why the braces are there
 * a. struct/union/enum "enum [name] {"
 * c. assignment "= {"
 * b. if/while/switch/for/etc ") {"
 * d. else "} else {"
 */

//#define DEBUG_NEWLINES
static void setup_newline_add(chunk_t *prev, chunk_t *nl, chunk_t *next)
{
   if (!prev || !nl || !next)
   {
      return;
   }

   undo_one_liner(prev);

   nl->orig_line   = prev->orig_line;
   nl->level       = prev->level;
   nl->brace_level = prev->brace_level;
   nl->pp_level    = prev->pp_level;
   nl->nl_count    = 1;
   nl->flags       = (prev->flags & PCF_COPY_FLAGS) & ~PCF_IN_PREPROC;
   if ((prev->flags & PCF_IN_PREPROC) && (next->flags & PCF_IN_PREPROC))
   {
      nl->flags |= PCF_IN_PREPROC;
   }
   if ((nl->flags & PCF_IN_PREPROC) != 0)
   {
      nl->type = CT_NL_CONT;
      nl->str  = "\\\n";
   }
   else
   {
      nl->type = CT_NEWLINE;
      nl->str  = "\n";
   }
}


/**
 * Add a newline before the chunk if there isn't already a newline present.
 * Virtual braces are skipped, as they do not contribute to the output.
 */
chunk_t *newline_add_before2(chunk_t *pc, const char *fcn, int line)
{
   chunk_t nl;
   chunk_t *prev;

   prev = chunk_get_prev_nvb(pc);
   if (chunk_is_newline(prev))
   {
      /* Already has a newline before this chunk */
      return(prev);
   }

   LOG_FMT(LNEWLINE, "%s: '%s' on line %d (called from %s line %d)\n",
           __func__, pc->str.c_str(), pc->orig_line, fcn, line);

   setup_newline_add(prev, &nl, pc);

   MARK_CHANGE();
   return(chunk_add_before(&nl, pc));
}


chunk_t *newline_force_before2(chunk_t *pc, const char *fcn, int line)
{
   chunk_t *nl = newline_add_before2(pc, fcn, line);
   if (nl && (nl->nl_count > 1))
   {
      nl->nl_count = 1;
      MARK_CHANGE();
   }
   return nl;
}


/**
 * Add a newline after the chunk if there isn't already a newline present.
 * Virtual braces are skipped, as they do not contribute to the output.
 */
chunk_t *newline_add_after2(chunk_t *pc, const char *fcn, int line)
{
   chunk_t nl;
   chunk_t *next;

   if (!pc)
   {
      return(NULL);
   }

   next = chunk_get_next_nvb(pc);
   if (chunk_is_newline(next))
   {
      /* Already has a newline after this chunk */
      return(next);
   }

   LOG_FMT(LNEWLINE, "%s: '%s' on line %d (called from %s line %d)\n",
           __func__, pc->str.c_str(), pc->orig_line, fcn, line);

   setup_newline_add(pc, &nl, next);

   MARK_CHANGE();
   return(chunk_add_after(&nl, pc));
}


chunk_t *newline_force_after2(chunk_t *pc, const char *fcn, int line)
{
   chunk_t *nl = newline_add_after2(pc, fcn, line);
   if (nl && (nl->nl_count > 1))
   {
      nl->nl_count = 1;
      MARK_CHANGE();
   }
   return nl;
}


/**
 * Clears the PCF_ONE_LINER flag on the current line.
 * Done right before inserting a newline.
 */
static void undo_one_liner(chunk_t *pc)
{
   chunk_t *tmp;

   if (pc && (pc->flags & PCF_ONE_LINER))
   {
      LOG_FMT(LNL1LINE, "%s: [%s]", __func__, pc->text());
      pc->flags &= ~PCF_ONE_LINER;

      /* scan backward */
      tmp = pc;
      while ((tmp = chunk_get_prev(tmp)) != NULL)
      {
         if (!(tmp->flags & PCF_ONE_LINER))
         {
            break;
         }
         LOG_FMT(LNL1LINE, " %s", tmp->text());
         tmp->flags &= ~PCF_ONE_LINER;
      }

      /* scan forward */
      tmp = pc;
      LOG_FMT(LNL1LINE, " -");
      while ((tmp = chunk_get_next(tmp)) != NULL)
      {
         if (!(tmp->flags & PCF_ONE_LINER))
         {
            break;
         }
         LOG_FMT(LNL1LINE, " %s", tmp->text());
         tmp->flags &= ~PCF_ONE_LINER;
      }
      LOG_FMT(LNL1LINE, "\n");
   }
}
