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


static bool one_liner_nl_ok(chunk_t *pc);
static void undo_one_liner(chunk_t *pc);
static void newline_iarf_pair(chunk_t *before, chunk_t *after, argval_t av);

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
 * Add a newline between two tokens.
 * If there is already a newline between then, nothing is done.
 * Otherwise a newline is inserted.
 *
 * If end is CT_BRACE_OPEN and a comment and newline follow, then
 * the brace open is moved instead of inserting a newline.
 *
 * In this situation:
 *    if (...) { //comment
 *
 * you get:
 *    if (...)   //comment
 *    {
 */
chunk_t *newline_add_between2(chunk_t *start, chunk_t *end,
                              const char *func, int line)
{
   chunk_t *pc;

   if ((start == NULL) || (end == NULL))
   {
      return(NULL);
   }

   LOG_FMT(LNEWLINE, "%s: '%s'[%s] line %d:%d and '%s' line %d:%d : caller=%s:%d\n",
           __func__, start->str.c_str(), get_token_name(start->type),
           start->orig_line, start->orig_col,
           end->str.c_str(), end->orig_line, end->orig_col, func, line);

   /* Back-up check for one-liners (should never be true!) */
   if (!one_liner_nl_ok(start))
   {
      return(NULL);
   }

   /* Scan for a line break */
   for (pc = start; pc != end; pc = chunk_get_next(pc))
   {
      if (chunk_is_newline(pc))
      {
         return(pc);
      }
   }

   /* If the second one is a brace open, then check to see
    * if a comment + newline follows
    */
   if (end->type == CT_BRACE_OPEN)
   {
      pc = chunk_get_next(end);
      if (chunk_is_comment(pc))
      {
         pc = chunk_get_next(pc);
         if (chunk_is_newline(pc))
         {
            /* Move the open brace to after the newline */
            chunk_move_after(end, pc);
            return(pc);
         }
      }
   }

   return(newline_add_before(end));
}


/**
 * Removes any CT_NEWLINE or CT_NL_CONT between start and end.
 * Start must be before end on the chunk list.
 * If the 'PCF_IN_PREPROC' status differs between two tags, we can't remove
 * the newline.
 *
 * @param start   The starting chunk (cannot be a newline)
 * @param end     The ending chunk (cannot be a newline)
 * @return        true/false - removed something
 */
void newline_del_between2(chunk_t *start, chunk_t *end,
                          const char *func, int line)
{
   chunk_t *next;
   chunk_t *prev;
   chunk_t *pc = start;

   LOG_FMT(LNEWLINE, "%s: '%s' line %d:%d and '%s' line %d:%d : caller=%s:%d preproc=%d/%d\n",
           __func__, start->str.c_str(), start->orig_line, start->orig_col,
           end->str.c_str(), end->orig_line, end->orig_col, func, line,
           ((start->flags & PCF_IN_PREPROC) != 0),
           ((end->flags & PCF_IN_PREPROC) != 0));

   /* Can't remove anything if the preproc status differs */
   if (!chunk_same_preproc(start, end))
   {
      return;
   }

   do
   {
      next = chunk_get_next(pc);
      if (chunk_is_newline(pc))
      {
         prev = chunk_get_prev(pc);
         if ((!chunk_is_comment(prev) && !chunk_is_comment(next)) ||
             chunk_is_newline(prev) ||
             chunk_is_newline(next))
         {
            if (chunk_safe_to_del_nl(pc))
            {
               chunk_del(pc);
               MARK_CHANGE();
               if (prev != NULL)
               {
                  align_to_column(next, prev->column + space_col_align(prev, next));
               }
            }
         }
         else
         {
            if (pc->nl_count > 1)
            {
               pc->nl_count = 1;
               MARK_CHANGE();
            }
         }
      }
      pc = next;
   } while (pc != end);

   if (chunk_is_str(end, "{", 1) &&
       (chunk_is_str(start, ")", 1) ||
        (start->type == CT_DO) ||
        (start->type == CT_ELSE)))
   {
      if (chunk_get_prev_nl(end) != start)
      {
         chunk_move_after(end, start);
      }
   }
}


/**
 * Does the Ignore, Add, Remove, or Force thing between two chunks
 *
 * @param before The first chunk
 * @param after  The second chunk
 * @param av     The IARF value
 */
static void newline_iarf_pair(chunk_t *before, chunk_t *after, argval_t av)
{
   if ((before != NULL) && (after != NULL))
   {
      if ((av & AV_ADD) != 0)
      {
         chunk_t *nl = newline_add_between(before, after);
         if (nl && (av == AV_FORCE) && (nl->nl_count > 1))
         {
            nl->nl_count = 1;
         }
      }
      else if ((av & AV_REMOVE) != 0)
      {
         newline_del_between(before, after);
      }
   }
}


/**
 * Does a simple Ignore, Add, Remove, or Force after the given chunk
 *
 * @param pc   The chunk
 * @param av   The IARF value
 */
void newline_iarf(chunk_t *pc, argval_t av)
{
   newline_iarf_pair(pc, chunk_get_next_nnl(pc), av);
}


/**
 * Checks to see if it is OK to add a newline around the chunk.
 * Don't want to break one-liners...
 */
static bool one_liner_nl_ok(chunk_t *pc)
{
   LOG_FMT(LNL1LINE, "%s: check [%s] parent=[%s] flg=%" PRIx64 ", on line %d, col %d - ",
           __func__, get_token_name(pc->type), get_token_name(pc->parent_type),
           pc->flags, pc->orig_line, pc->orig_col);

   if (!(pc->flags & PCF_ONE_LINER))
   {
      LOG_FMT(LNL1LINE, "true (not 1-liner)\n");
      return(true);
   }

   /* Step back to find the opening brace */
   chunk_t *br_open = pc;
   if (chunk_is_closing_brace(br_open))
   {
      br_open = chunk_get_prev_type(br_open,
                                    br_open->type == CT_BRACE_CLOSE ? CT_BRACE_OPEN : CT_VBRACE_OPEN,
                                    br_open->level, CNAV_ALL);
   }
   else
   {
      while (br_open &&
             (br_open->flags & PCF_ONE_LINER) &&
             !chunk_is_opening_brace(br_open) &&
             !chunk_is_closing_brace(br_open))
      {
         br_open = chunk_get_prev(br_open);
      }
   }
   pc = br_open;
   if (pc && (pc->flags & PCF_ONE_LINER) &&
       ((pc->type == CT_BRACE_OPEN) ||
        (pc->type == CT_BRACE_CLOSE) ||
        (pc->type == CT_VBRACE_OPEN) ||
        (pc->type == CT_VBRACE_CLOSE)))
   {
      if (cpd.settings[UO_nl_class_leave_one_liners].b &&
          (pc->flags & PCF_IN_CLASS))
      {
         LOG_FMT(LNL1LINE, "false (class)\n");
         return(false);
      }

      if (cpd.settings[UO_nl_assign_leave_one_liners].b &&
          (pc->parent_type == CT_ASSIGN))
      {
         LOG_FMT(LNL1LINE, "false (assign)\n");
         return(false);
      }

      if (cpd.settings[UO_nl_enum_leave_one_liners].b &&
          (pc->parent_type == CT_ENUM))
      {
         LOG_FMT(LNL1LINE, "false (enum)\n");
         return(false);
      }

      if (cpd.settings[UO_nl_getset_leave_one_liners].b &&
          (pc->parent_type == CT_GETSET))
      {
         LOG_FMT(LNL1LINE, "false (get/set)\n");
         return(false);
      }

      if (cpd.settings[UO_nl_func_leave_one_liners].b &&
          ((pc->parent_type == CT_FUNC_DEF) ||
           (pc->parent_type == CT_FUNC_CLASS)))
      {
         LOG_FMT(LNL1LINE, "false (func def)\n");
         return(false);
      }

      if (cpd.settings[UO_nl_func_leave_one_liners].b &&
          (pc->parent_type == CT_OC_MSG_DECL))
      {
         LOG_FMT(LNL1LINE, "false (method def)\n");
         return(false);
      }

      if (cpd.settings[UO_nl_cpp_lambda_leave_one_liners].b &&
          ((pc->parent_type == CT_CPP_LAMBDA)))
      {
         LOG_FMT(LNL1LINE, "false (lambda)\n");
         return(false);
      }

      if (cpd.settings[UO_nl_oc_msg_leave_one_liner].b &&
          (pc->flags & PCF_IN_OC_MSG))
      {
         LOG_FMT(LNL1LINE, "false (message)\n");
         return(false);
      }

      if (cpd.settings[UO_nl_if_leave_one_liners].b &&
          ((pc->parent_type == CT_IF) ||
           (pc->parent_type == CT_ELSE)))
      {
         LOG_FMT(LNL1LINE, "false (if/else)\n");
         return(false);
      }
   }
   LOG_FMT(LNL1LINE, "true\n");
   return(true);
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


void annotations_newlines(void)
{
   chunk_t *pc;
   chunk_t *next;
   chunk_t *prev;
   chunk_t *ae;   /* last token of the annotation */

   pc = chunk_get_head();
   while (((pc = chunk_get_next_type(pc, CT_ANNOTATION, -1)) != NULL) &&
          ((next = chunk_get_next_nnl(pc)) != NULL))
   {
      /* find the end of this annotation */
      if (chunk_is_paren_open(next))
      {
         /* TODO: control newline between annotation and '(' ? */
         ae = chunk_skip_to_match(next);
      }
      else
      {
         ae = pc;
      }
      if (!ae)
      {
         break;
      }

      prev = chunk_get_prev(pc);

      LOG_FMT(LANNOT, "%s: %d:%d annotation '%s' end@%d:%d '%s'",
              __func__, pc->orig_line, pc->orig_col, pc->text(),
              ae->orig_line, ae->orig_col, ae->text());

      if (chunk_is_token(prev, CT_ANNOTATION) ||
          !chunk_is_newline(prev))
      {
         /* skip it */
         LOG_FMT(LANNOT, " -- ignored\n");
      }
      else
      {
         next = chunk_get_next_nnl(ae);
         if (chunk_is_token(next, CT_ANNOTATION))
         {
            LOG_FMT(LANNOT, " -- nl_between_annotation\n");
            newline_iarf(ae, cpd.settings[UO_nl_between_annotation].a);
         }
         else
         {
            LOG_FMT(LANNOT, " -- nl_after_annotation\n");
            newline_iarf(ae, cpd.settings[UO_nl_after_annotation].a);
         }
      }
   }
}
