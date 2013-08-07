/**
 * @file space.cpp
 * Adds or removes inter-chunk spaces.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "toks_types.h"
#include "chunk_list.h"
#include "prototypes.h"
#include "char_table.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include "unc_ctype.h"


struct no_space_table_s
{
   c_token_t first;
   c_token_t second;
};

/** this table lists out all combos where a space should NOT be present
 * CT_UNKNOWN is a wildcard.
 *
 * TODO: some of these are no longer needed.
 */
struct no_space_table_s no_space_table[] =
{
   { CT_OC_AT,          CT_UNKNOWN       },
   { CT_INCDEC_BEFORE,  CT_WORD          },
   { CT_UNKNOWN,        CT_INCDEC_AFTER  },
   { CT_UNKNOWN,        CT_LABEL_COLON   },
   { CT_UNKNOWN,        CT_PRIVATE_COLON },
   { CT_UNKNOWN,        CT_SEMICOLON     },
   { CT_UNKNOWN,        CT_D_TEMPLATE    },
   { CT_D_TEMPLATE,     CT_UNKNOWN       },
   { CT_MACRO_FUNC,     CT_FPAREN_OPEN   },
   { CT_PAREN_OPEN,     CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_PAREN_CLOSE   },
   { CT_FPAREN_OPEN,    CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_SPAREN_CLOSE  },
   { CT_SPAREN_OPEN,    CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_FPAREN_CLOSE  },
   { CT_UNKNOWN,        CT_COMMA         },
   { CT_POS,            CT_UNKNOWN       },
   { CT_STAR,           CT_UNKNOWN       },
   { CT_VBRACE_CLOSE,   CT_UNKNOWN       },
   { CT_VBRACE_OPEN,    CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_VBRACE_CLOSE  },
   { CT_UNKNOWN,        CT_VBRACE_OPEN   },
   { CT_PREPROC,        CT_UNKNOWN       },
   { CT_PREPROC_INDENT, CT_UNKNOWN       },
   { CT_NEG,            CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_SQUARE_OPEN   },
   { CT_UNKNOWN,        CT_SQUARE_CLOSE  },
   { CT_SQUARE_OPEN,    CT_UNKNOWN       },
   { CT_PAREN_CLOSE,    CT_WORD          },
   { CT_PAREN_CLOSE,    CT_FUNC_DEF      },
   { CT_PAREN_CLOSE,    CT_FUNC_CALL     },
   { CT_PAREN_CLOSE,    CT_ADDR          },
   { CT_PAREN_CLOSE,    CT_FPAREN_OPEN   },
   { CT_OC_SEL_NAME,    CT_OC_SEL_NAME   },
};


void space_add_after(chunk_t *pc, int count)
{
   if (count <= 0)
   {
      return;
   }

   chunk_t *next = chunk_get_next(pc);

   /* don't add at the end of the file or before a newline */
   if ((next == NULL) || chunk_is_newline(next))
   {
      return;
   }

   /* Limit to 16 spaces */
   if (count > 16)
   {
      count = 16;
   }

   /* Two CT_SPACE in a row -- use the max of the two */
   if (next->type == CT_SPACE)
   {
      if (next->len() < count)
      {
         while (next->len() < count)
         {
            next->str.append(' ');
         }
      }
      return;
   }

   chunk_t sp;

   sp.flags       = pc->flags & PCF_COPY_FLAGS;
   sp.type        = CT_SPACE;
   sp.str         = "                "; // 16 spaces
   sp.str.resize(count);
   sp.level       = pc->level;
   sp.brace_level = pc->brace_level;
   sp.pp_level    = pc->pp_level;
   sp.column      = pc->column + pc->len();
   sp.orig_line   = pc->orig_line;

   chunk_add_after(&sp, pc);
}
