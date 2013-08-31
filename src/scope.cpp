/**
 * @file scope.cpp
 * Add scope information.
 *
 * @author  Thomas Thorsen
 * @license GPL v2+
 */
#include "toks_types.h"
#include "chunk_list.h"
#include "unc_text.h"
#include "prototypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include "unc_ctype.h"
#include <cassert>

static void mark_resolved_scopes(chunk_t *pc, unc_text& res_scopes)
{
   if (res_scopes.size() > 0)
   {
      if (pc->scope.size() > 0)
      {
         pc->scope.append(":");
      }
      pc->scope.append(res_scopes);
   }
}

static void mark_scope_single(chunk_t *pc,
                              chunk_t *scope,
                              const char *decoration,
                              unc_text& res_scopes)
{
   if (pc->scope.size() > 0)
   {
      pc->scope.append(":");
   }

   if (res_scopes.size() > 0)
   {
      pc->scope.append(res_scopes);
      pc->scope.append(":");
   }

   if ((scope->type == CT_FUNC_CLASS) &&
       (scope->parent_type == CT_DESTRUCTOR))
   {
      pc->scope.append("~");
   }

   pc->scope.append(scope->text());

   if (decoration != NULL)
   {
      pc->scope.append(decoration);
   }
}

static chunk_t *mark_scope(chunk_t *popen,
                           chunk_t *scope,
                           const char *decoration,
                           unc_text& res_scopes)
{
   chunk_t *pc = popen;

   while (pc != NULL)
   {
      mark_scope_single(pc, scope, decoration, res_scopes);

      if (((pc->type == (popen->type + 1)) &&
          ((pc->level == popen->level) ||
          (popen->level < 0))))
      {
         break;
      }

      pc = chunk_get_next(pc, CNAV_PREPROC);
   }

   return pc;
}


static void get_resolved_scopes(chunk_t *scope, unc_text& res_scopes)
{
   chunk_t *prev = chunk_get_prev_ncnl(scope, CNAV_PREPROC);
   bool first = true;

   res_scopes.clear();

   if ((scope->type == CT_FUNC_CLASS) &&
       (scope->parent_type == CT_DESTRUCTOR))
   {
      prev = chunk_get_prev_ncnl(prev, CNAV_PREPROC);
   }

   while ((prev != NULL) && (prev->type == CT_DC_MEMBER))
   {
      prev = chunk_get_prev_ncnl(prev, CNAV_PREPROC);
      if (prev->type != CT_TYPE)
         break;
      if (!first)
      {
         res_scopes.prepend(":");
      }
      first = false;
      res_scopes.prepend(prev->str);
      prev = chunk_get_prev_ncnl(prev, CNAV_PREPROC);
   }
}


void assign_scope()
{
   chunk_t *pc = chunk_get_head();
   unc_text res_scopes;

   while (pc != NULL)
   {
      get_resolved_scopes(pc, res_scopes);

      switch (pc->type)
      {
         case CT_WORD:
         {
            if ((pc->parent_type != CT_NAMESPACE) &&
                (pc->flags & PCF_DEF))
            {
               chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

               mark_resolved_scopes(pc, res_scopes);

               if (next->type == CT_BRACE_OPEN)
               {
                  mark_scope(next, pc, NULL, res_scopes);
               }
            }
            break;
         }
         case CT_TYPE:
         {
            if (((pc->parent_type == CT_CLASS) ||
                 (pc->parent_type == CT_STRUCT) ||
                 (pc->parent_type == CT_UNION) ||
                 (pc->parent_type == CT_ENUM)) &&
                (pc->flags & PCF_DEF))
            {
               chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

               mark_resolved_scopes(pc, res_scopes);

               if (next->type == CT_BRACE_OPEN)
               {
                  mark_scope(next, pc, NULL, res_scopes);
               }
            }
            break;
         }
         case CT_FUNC_PROTO:
         {
            chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

            mark_resolved_scopes(pc, res_scopes);

            if (next->type == CT_FPAREN_OPEN)
            {
               mark_scope(next, pc, "()", res_scopes);
            }
            break;
         }
         case CT_FUNC_DEF:
         {
            chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

            mark_resolved_scopes(pc, res_scopes);

            if (next->type == CT_FPAREN_OPEN)
            {
               next = mark_scope(next, pc, "()", res_scopes);
            }

            next = chunk_get_next_ncnl(next, CNAV_PREPROC);

            /* Skip const/volatile */
            while ((next != NULL) && (next->type == CT_QUALIFIER))
            {
               next = chunk_get_next_ncnl(next, CNAV_PREPROC);
            }

            if ((next != NULL) && (next->type == CT_BRACE_OPEN))
            {
               mark_scope(next, pc, "{}", res_scopes);
            }
            break;
         }
         case CT_FUNC_CLASS:
         {
            if (pc->flags & (PCF_DEF | PCF_PROTO))
            {
               chunk_t *next = chunk_get_next_ncnl(pc, CNAV_PREPROC);

               mark_resolved_scopes(pc, res_scopes);

               if (next->type == CT_FPAREN_OPEN)
               {
                  next = mark_scope(next, pc, "()", res_scopes);
               }

               if (pc->flags & PCF_DEF)
               {
                  /* Skip default args (while marking them) */
                  while ((next != NULL) && (next->flags & PCF_IN_CONST_ARGS))
                  {
                     mark_scope_single(next, pc, "()", res_scopes);
                     next = chunk_get_next_ncnl(next, CNAV_PREPROC);
                  }

                  if ((next != NULL) && (next->type == CT_BRACE_OPEN))
                  {
                     mark_scope(next, pc, "{}", res_scopes);
                  }
               }
            }
            break;
         }
         default:
            break;
      }

      if (pc->scope.size() == 0)
      {
         if (pc->flags & PCF_STATIC)
         {
            pc->scope.set("<local>");
         }
         else if (pc->flags & PCF_IN_PREPROC)
         {
            pc->scope.set("<preproc>");
         }
         else
         {
            pc->scope.set("<global>");
         }
      }

      pc = chunk_get_next(pc);
    }
}
