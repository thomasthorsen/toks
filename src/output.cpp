/**
 * @file output.cpp
 * Does all the output & comment formatting.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#define DEFINE_PCF_NAMES
#include "toks_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include <cstdlib>

void output(void)
{
   chunk_t *pc;
   for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
   {
      switch (pc->type)
      {
         case CT_MACRO_FUNC:
         case CT_FUNC_DEF:
         case CT_FUNC_PROTO:
         case CT_FUNC_CALL:
         {
            printf("%s:%u:%u %s %s ", cpd.filename, pc->orig_line, pc->orig_col, get_token_name(pc->type), pc->str.c_str());

            /* Context */
            for (chunk_t *tmp = pc;
                 tmp != NULL;
                 tmp = chunk_get_next(tmp))
            {
               if ((tmp->type == CT_WORD) && (tmp->flags & PCF_VAR_DEF) && (tmp->flags & PCF_IN_FCN_DEF))
                  printf(" ");
               printf("%s", tmp->str.c_str());
               if ((tmp->type == CT_COMMA) && (tmp->flags & PCF_IN_FCN_DEF))
                  printf(" ");
               if ((tmp->level == pc->level) &&
                   (tmp->type == CT_FPAREN_CLOSE) &&
                   (tmp->parent_type == pc->type))
                  break;
            }

            printf("\n");
            break;
         }
         case CT_MACRO:
         {
            printf("%s:%u:%u %s %s #define ", cpd.filename, pc->orig_line, pc->orig_col, get_token_name(pc->type), pc->str.c_str());

            /* Context */
            for (chunk_t *tmp = pc;
                 (tmp != NULL) && (tmp->flags & PCF_IN_PREPROC);
                 tmp = chunk_get_next(tmp))
            {
               if ((tmp->type != CT_NEWLINE) && (tmp->type != CT_NL_CONT) && (tmp->len() != 0))
               {
                  printf("%s ", tmp->str.c_str());
               }
            }
            printf("\n");
            break;
         }
         case CT_TYPE:
         {
            if (pc->parent_type == CT_TYPEDEF)
            {
               printf("%s:%u:%u %s", cpd.filename, pc->orig_line, pc->orig_col, get_token_name(pc->parent_type));
               if (pc->flags & PCF_TYPEDEF_STRUCT)
                  printf("_STRUCT");
               else if (pc->flags & PCF_TYPEDEF_UNION)
                  printf("_UNION");
               else if (pc->flags & PCF_TYPEDEF_ENUM)
                  printf("_ENUM");
               printf(" %s\n", pc->str.c_str());
            }
            if (pc->parent_type == CT_STRUCT ||
                pc->parent_type == CT_UNION ||
                pc->parent_type == CT_ENUM)
            {
               printf("%s:%u:%u %s", cpd.filename, pc->orig_line, pc->orig_col, get_token_name(pc->parent_type));
               if (pc->flags & PCF_DEF)
                  printf("_DEF");
               else if (pc->flags & PCF_PROTO)
                  printf("_PROTO");
               else if (pc->flags & PCF_REF)
                  printf("_REF");
               printf(" %s\n", pc->str.c_str());
            }
            break;
         }
         case CT_FUNC_TYPE:
         {
            if (pc->parent_type == CT_TYPEDEF)
            {
               printf("%s:%u:%u TYPEDEF_FUNC %s\n", cpd.filename, pc->orig_line, pc->orig_col, pc->str.c_str());
            }
         }
         case CT_WORD:
         {
            if (pc->flags & PCF_IN_ENUM)
            {
               printf("%s:%u:%u ENUM_VAL %s\n", cpd.filename, pc->orig_line, pc->orig_col, pc->str.c_str());
            }
            break;
         }
         default:
            break;
      }
   }
}

void output_parsed(FILE *pfile)
{
   chunk_t *pc;
   int     cnt;
   const char *tolog;

   fprintf(pfile, "# -=====-\n");
   fprintf(pfile, "# Line      Tag          Parent     Columns  Br/Lvl/pp Flag Nl  Text");
   for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
   {
      fprintf(pfile, "\n# %3d> %13.13s[%13.13s][%2d/%2d/%2d][%d/%d/%d][%10" PRIx64 "][%d-%d]",
              pc->orig_line, get_token_name(pc->type),
              get_token_name(pc->parent_type),
              pc->column, pc->orig_col, pc->orig_col_end,
              pc->brace_level, pc->level, pc->pp_level,
              pc->flags, pc->nl_count, pc->after_tab);

      tolog = NULL;
      for (int i = 0; i < (int)ARRAY_SIZE(pcf_names); i++)
      {
       if ((pc->flags & (1ULL << i)) != 0)
       {
          if (tolog != NULL)
          {
             fprintf(pfile, "%s", tolog);
             fprintf(pfile, ",");
          }
          tolog = pcf_names[i];
       }
      }

      if (tolog != NULL)
      {
       fprintf(pfile, "%s", tolog);
      }

      if ((pc->type != CT_NEWLINE) && (pc->len() != 0))
      {
         for (cnt = 0; cnt < pc->column; cnt++)
         {
            fprintf(pfile, " ");
         }
         if (pc->type != CT_NL_CONT)
         {
            fprintf(pfile, "%s", pc->str.c_str());
         }
         else
         {
            fprintf(pfile, "\\");
         }
      }
   }
   fprintf(pfile, "\n# -=====-\n");
   fflush(pfile);
}
