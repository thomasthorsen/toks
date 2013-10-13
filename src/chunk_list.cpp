/**
 * @file chunk_list.cpp
 * Manages and navigates the list of chunks.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "chunk_list.h"
#include <cstring>
#include <cstdlib>

#include "ListManager.h"
#include "prototypes.h"


chunk_t *chunk_get_head(fp_data& fpd)
{
   return(fpd.chunk_list.GetHead());
}


chunk_t *chunk_get_tail(fp_data& fpd)
{
   return(fpd.chunk_list.GetTail());
}


chunk_t *chunk_get_next(chunk_t *cur, chunk_nav_t nav)
{
   if (cur == NULL)
   {
      return(NULL);
   }
   chunk_t *pc = cur->next;
   if ((pc == NULL) || (nav == CNAV_ALL))
   {
      return(pc);
   }
   if (cur->flags & PCF_IN_PREPROC)
   {
      /* If in a preproc, return NULL if trying to leave */
      if ((pc->flags & PCF_IN_PREPROC) == 0)
      {
         return(NULL);
      }
      return(pc);
   }
   /* Not in a preproc, skip any preproc */
   while ((pc != NULL) && (pc->flags & PCF_IN_PREPROC))
   {
      pc = pc->next;
   }
   return(pc);
}


chunk_t *chunk_get_prev(chunk_t *cur, chunk_nav_t nav)
{
   if (cur == NULL)
   {
      return(NULL);
   }
   chunk_t *pc = cur->prev;
   if ((pc == NULL) || (nav == CNAV_ALL))
   {
      return(pc);
   }
   if (cur->flags & PCF_IN_PREPROC)
   {
      /* If in a preproc, return NULL if trying to leave */
      if ((pc->flags & PCF_IN_PREPROC) == 0)
      {
         return(NULL);
      }
      return(pc);
   }
   /* Not in a preproc, skip any proproc */
   while ((pc != NULL) && (pc->flags & PCF_IN_PREPROC))
   {
      pc = pc->prev;
   }
   return(pc);
}


chunk_t *chunk_dup(const chunk_t *pc_in)
{
   chunk_t *pc;

   /* Allocate the entry */
   pc = new chunk_t;
   if (pc == NULL)
   {
      exit(1);
   }

   /* Copy all fields and then init the entry */
   *pc = *pc_in;
   pc->next = NULL;
   pc->prev = NULL;

   return(pc);
}


/**
 * Add to the tail of the list
 */
chunk_t *chunk_add(fp_data& fpd, const chunk_t *pc_in)
{
   chunk_t *pc;

   if ((pc = chunk_dup(pc_in)) != NULL)
   {
      fpd.chunk_list.AddTail(pc);
   }
   return(pc);
}


/**
 * Add a copy after the given chunk.
 * If ref is NULL, add at the head.
 */
chunk_t *chunk_add_after(fp_data& fpd, const chunk_t *pc_in, chunk_t *ref)
{
   chunk_t *pc;

   if ((pc = chunk_dup(pc_in)) != NULL)
   {
      if (ref != NULL)
      {
         fpd.chunk_list.AddAfter(pc, ref);
      }
      else
      {
         fpd.chunk_list.AddHead(pc);
      }
   }
   return(pc);
}


/**
 * Add a copy before the given chunk.
 * If ref is NULL, add at the head.
 */
chunk_t *chunk_add_before(fp_data& fpd, const chunk_t *pc_in, chunk_t *ref)
{
   chunk_t *pc;

   if ((pc = chunk_dup(pc_in)) != NULL)
   {
      if (ref != NULL)
      {
         fpd.chunk_list.AddBefore(pc, ref);
      }
      else
      {
         fpd.chunk_list.AddTail(pc);
      }
   }
   return(pc);
}


void chunk_del(fp_data& fpd, chunk_t *pc)
{
   fpd.chunk_list.Pop(pc);
   delete pc;
}


/**
 * Gets the next NEWLINE chunk
 */
chunk_t *chunk_get_next_nl(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_next(pc, nav);
   } while ((pc != NULL) && !chunk_is_newline(pc));
   return(pc);
}


/**
 * Gets the prev NEWLINE chunk
 */
chunk_t *chunk_get_prev_nl(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_prev(pc, nav);
   } while ((pc != NULL) && !chunk_is_newline(pc));
   return(pc);
}


/**
 * Gets the next non-NEWLINE chunk
 */
chunk_t *chunk_get_next_nnl(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_next(pc, nav);
   } while (chunk_is_newline(pc));
   return(pc);
}


/**
 * Gets the prev non-NEWLINE chunk
 */
chunk_t *chunk_get_prev_nnl(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_prev(pc, nav);
   } while ((pc != NULL) && chunk_is_newline(pc));
   return(pc);
}


/**
 * Gets the next non-NEWLINE and non-comment chunk
 */
chunk_t *chunk_get_next_ncnl(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_next(pc, nav);
   } while ((pc != NULL) && (chunk_is_comment(pc) || chunk_is_newline(pc)));
   return(pc);
}


/**
 * Gets the next non-NEWLINE and non-comment chunk, non-preprocessor chunk
 */
chunk_t *chunk_get_next_ncnlnp(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   if (chunk_is_preproc(cur))
   {
      do
      {
         pc = chunk_get_next(pc, nav);
      } while ((pc != NULL) && chunk_is_preproc(pc) &&
               (chunk_is_comment(pc) || chunk_is_newline(pc)));
   }
   else
   {
      do
      {
         pc = chunk_get_next(pc, nav);
      } while ((pc != NULL) && (chunk_is_comment(pc) ||
                                chunk_is_newline(pc) ||
                                chunk_is_preproc(pc)));
   }
   return(pc);
}


/**
 * Gets the prev non-NEWLINE and non-comment chunk, non-preprocessor chunk
 */
chunk_t *chunk_get_prev_ncnlnp(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   if (chunk_is_preproc(cur))
   {
      do
      {
         pc = chunk_get_prev(pc, nav);
      } while ((pc != NULL) && chunk_is_preproc(pc) &&
               (chunk_is_comment(pc) || chunk_is_newline(pc)));
   }
   else
   {
      do
      {
         pc = chunk_get_prev(pc, nav);
      } while ((pc != NULL) && (chunk_is_comment(pc) ||
                                chunk_is_newline(pc) ||
                                chunk_is_preproc(pc)));
   }
   return(pc);
}


/**
 * Gets the next non-blank chunk
 */
chunk_t *chunk_get_next_nblank(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_next(pc, nav);
   } while ((pc != NULL) && (chunk_is_comment(pc) ||
                             chunk_is_newline(pc) ||
                             chunk_is_blank(pc)));
   return(pc);
}


/**
 * Gets the prev non-blank chunk
 */
chunk_t *chunk_get_prev_nblank(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_prev(pc, nav);
   } while ((pc != NULL) && (chunk_is_comment(pc) || chunk_is_newline(pc) ||
                             chunk_is_blank(pc)));
   return(pc);
}


/**
 * Gets the next non-comment chunk
 */
chunk_t *chunk_get_next_nc(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_next(pc, nav);
   } while ((pc != NULL) && chunk_is_comment(pc));
   return(pc);
}


/**
 * Gets the prev non-NEWLINE and non-comment chunk
 */
chunk_t *chunk_get_prev_ncnl(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_prev(pc, nav);
   } while ((pc != NULL) && (chunk_is_comment(pc) || chunk_is_newline(pc)));
   return(pc);
}


/**
 * Gets the prev non-comment chunk
 */
chunk_t *chunk_get_prev_nc(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_prev(pc, nav);
   } while ((pc != NULL) && chunk_is_comment(pc));
   return(pc);
}


/**
 * Grabs the next chunk of the given type at the level.
 *
 * @param cur     Starting chunk
 * @param type    The type to look for
 * @param level   -1 (any level) or the level to match
 * @return        NULL or the match
 */
chunk_t *chunk_get_next_type(chunk_t *cur, c_token_t type,
                             int level, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_next(pc, nav);
      if ((pc == NULL) ||
          ((pc->type == type) && ((pc->level == level) || (level < 0))))
      {
         break;
      }
   } while (pc != NULL);
   return(pc);
}


chunk_t *chunk_get_next_str(chunk_t *cur, const char *str, int len, int level,
                            chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_next(pc, nav);
      if ((pc == NULL) ||
          ((pc->len() == len) && (memcmp(str, pc->text(), len) == 0) &&
           ((pc->level == level) || (level < 0))))
      {
         break;
      }
   } while (pc != NULL);
   return(pc);
}


/**
 * Grabs the prev chunk of the given type at the level.
 *
 * @param cur     Starting chunk
 * @param type    The type to look for
 * @param level   -1 (any level) or the level to match
 * @return        NULL or the match
 */
chunk_t *chunk_get_prev_type(chunk_t *cur, c_token_t type,
                             int level, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_prev(pc, nav);
      if ((pc == NULL) ||
          ((pc->type == type) && ((pc->level == level) || (level < 0))))
      {
         break;
      }
   } while (pc != NULL);
   return(pc);
}


chunk_t *chunk_get_prev_str(chunk_t *cur, const char *str, int len, int level,
                            chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_prev(pc, nav);
      if ((pc == NULL) ||
          ((pc->len() == len) && (memcmp(str, pc->text(), len) == 0) &&
           ((pc->level == level) || (level < 0))))
      {
         break;
      }
   } while (pc != NULL);
   return(pc);
}


/**
 * Check to see if there is a newline bewteen the two chunks
 */
bool chunk_is_newline_between(chunk_t *start, chunk_t *end)
{
   chunk_t *pc;

   for (pc = start; pc != end; pc = chunk_get_next(pc))
   {
      if (chunk_is_newline(pc))
      {
         return(true);
      }
   }
   return(false);
}


/**
 * Finds the first chunk on the line that pc is on.
 * This just backs up until a newline or NULL is hit.
 *
 * given: [ a - b - c - n1 - d - e - n2 ]
 * input: [ a | b | c | n1 ] => a
 * input: [ d | e | n2 ]     => d
 */
chunk_t *chunk_first_on_line(chunk_t *pc)
{
   chunk_t *first = pc;

   while (((pc = chunk_get_prev(pc)) != NULL) && !chunk_is_newline(pc))
   {
      first = pc;
   }

   return(first);
}


/**
 * Gets the next non-vbrace chunk
 */
chunk_t *chunk_get_next_nvb(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_next(pc, nav);
   } while (chunk_is_vbrace(pc));
   return(pc);
}


/**
 * Gets the prev non-vbrace chunk
 */
chunk_t *chunk_get_prev_nvb(chunk_t *cur, chunk_nav_t nav)
{
   chunk_t *pc = cur;

   do
   {
      pc = chunk_get_prev(pc, nav);
   } while (chunk_is_vbrace(pc));
   return(pc);
}
