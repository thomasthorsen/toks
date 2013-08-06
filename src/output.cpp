/**
 * @file output.cpp
 * Does all the output & comment formatting.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "toks_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include <cstdlib>

void output_parsed(FILE *pfile)
{
   chunk_t *pc;
   int     cnt;

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
