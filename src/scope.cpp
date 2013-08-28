/**
 * @file scope.cpp
 * Add scope information.
 *
 * @author  Thomas Thorsen
 * @license GPL v2+
 */
#include "toks_types.h"
#include "chunk_list.h"
#include "ChunkStack.h"
#include "prototypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include "unc_ctype.h"
#include <cassert>

static chunk_t *mark_scope(chunk_t *cur, chunk_t *scope)
{
   chunk_t *pc = cur;

   while (pc != NULL)
   {
      if (pc->scope.size() > 0)
      {
         pc->scope.append(":");
      }
      pc->scope.append(scope->text());

      if (((pc->type == (cur->type + 1)) && ((pc->level == cur->level) || (cur->level < 0))))
      {
         break;
      }

      pc = chunk_get_next(pc, CNAV_PREPROC);
   }

   return pc;
}

void assign_scope()
{
   chunk_t *pc = chunk_get_head();

   while (pc != NULL)
   {
      /* Namespace */
      if ((pc->type == CT_WORD) && (pc->parent_type == CT_NAMESPACE) && (pc->flags & PCF_DEF))
      {
         chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

         if (next->type == CT_BRACE_OPEN)
         {
            mark_scope(next, pc);
         }
      }

      /* Function prototype */
      if (pc->type == CT_FUNC_PROTO)
      {
         chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

         if (next->type == CT_FPAREN_OPEN)
         {
            mark_scope(next, pc);
         }
      }

      /* Function definition */
      if (pc->type == CT_FUNC_DEF)
      {
         chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

         if (next->type == CT_FPAREN_OPEN)
         {
            next = mark_scope(next, pc);
         }

         next = chunk_get_next_ncnl(next, CNAV_PREPROC);

         if (next->type == CT_BRACE_OPEN)
         {
            mark_scope(next, pc);
         }
      }

      /* Class definition */
      if ((pc->type == CT_TYPE) && (pc->parent_type == CT_CLASS) && (pc->flags & PCF_DEF))
      {
         chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

         if (next->type == CT_BRACE_OPEN)
         {
            mark_scope(next, pc);
         }
      }

      if (pc->scope.size() == 0)
      {
         pc->scope.set("<global>");
      }

      pc = chunk_get_next(pc);
    }
}
