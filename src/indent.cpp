/**
 * @file indent.cpp
 * Does all the indenting stuff.
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


/**
 * General indenting approach:
 * Indenting levels are put into a stack.
 *
 * The stack entries contain:
 *  - opening type
 *  - brace column
 *  - continuation column
 *
 * Items that start a new stack item:
 *  - preprocessor (new parse frame)
 *  - Brace Open (Virtual brace also)
 *  - Paren, Square, Angle open
 *  - Assignments
 *  - C++ '<<' operator (ie, cout << "blah")
 *  - case
 *  - class colon
 *  - return
 *  - types
 *  - any other continued statement
 *
 * Note that the column of items marked 'PCF_WAS_ALIGNED' is not changed.
 *
 * For an open brace:
 *  - indent increases by indent_columns
 *  - if part of if/else/do/while/switch/etc, an extra indent may be applied
 *  - if in a paren, then cont-col is set to column + 1, ie "({ some code })"
 *
 * Open paren/square/angle:
 * cont-col is set to the column of the item after the open paren, unless
 * followed by a newline, then it is set to (brace-col + indent_columns).
 * Examples:
 *    a_really_long_funcion_name(
 *       param1, param2);
 *    a_really_long_funcion_name(param1,
 *                               param2);
 *
 * Assignments:
 * Assignments are continued aligned with the first item after the assignment,
 * unless the assign is followed by a newline.
 * Examples:
 *    some.variable = asdf + asdf +
 *                    asdf;
 *    some.variable =
 *       asdf + asdf + asdf;
 *
 * C++ << operator:
 * Handled the same as assignment.
 * Examples:
 *    cout << "this is test number: "
 *         << test_number;
 *
 * case:
 * Started with case or default.
 * Terminated with close brace at level or another case or default.
 * Special indenting according to various rules.
 *  - indent of case label
 *  - indent of case body
 *  - how to handle optional braces
 * Examples:
 * {
 * case x: {
 *    a++;
 *    break;
 *    }
 * case y:
 *    b--;
 *    break;
 * default:
 *    c++;
 *    break;
 * }
 *
 * Class colon:
 * Indent continuation by indent_columns:
 * class my_class :
 *    baseclass1,
 *    baseclass2
 * {
 *
 * Return: same as assignemts
 * If the return statement is not fully paren'd, then the indent continues at
 * the column of the item after the return. If it is paren'd, then the paren
 * rules apply.
 * return somevalue +
 *        othervalue;
 *
 * Type: pretty much the same as assignments
 * Examples:
 * int foo,
 *     bar,
 *     baz;
 *
 * Any other continued item:
 * There shouldn't be anything not covered by the above cases, but any other
 * continued item is indented by indent_columns:
 * Example:
 * somereallycrazylongname.with[lotsoflongstuff].
 *    thatreallyannoysme.whenIhavetomaintain[thecode] = 3;
 */



void indent_to_column(chunk_t *pc, int column)
{
   if (column < pc->column)
   {
      column = pc->column;
   }
   reindent_line(pc, column);
}


enum align_mode
{
   ALMODE_SHIFT,     /* shift relative to the current column */
   ALMODE_KEEP_ABS,  /* try to keep the original absolute column */
   ALMODE_KEEP_REL,  /* try to keep the original gap */
};

/* Same as indent_to_column, except we can move both ways */
void align_to_column(chunk_t *pc, int column)
{
   if (column == pc->column)
   {
      return;
   }

   LOG_FMT(LINDLINE, "%s: %d] col %d on %s [%s] => %d\n",
           __func__, pc->orig_line, pc->column, pc->str.c_str(),
           get_token_name(pc->type), column);

   int col_delta = column - pc->column;
   int min_col   = column;
   int min_delta;

   pc->column = column;
   do
   {
      chunk_t    *next = chunk_get_next(pc);
      chunk_t    *prev;
      align_mode almod = ALMODE_SHIFT;

      if (next == NULL)
      {
         break;
      }
      min_delta = space_col_align(pc, next);
      min_col  += min_delta;
      prev      = pc;
      pc        = next;

      if (chunk_is_comment(pc) && (pc->parent_type != CT_COMMENT_EMBED))
      {
         almod = (chunk_is_single_line_comment(pc) &&
                  cpd.settings[UO_indent_relative_single_line_comments].b) ?
                 ALMODE_KEEP_REL : ALMODE_KEEP_ABS;
      }

      if (almod == ALMODE_KEEP_ABS)
      {
         /* Keep same absolute column */
         pc->column = pc->orig_col;
         if (pc->column < min_col)
         {
            pc->column = min_col;
         }
      }
      else if (almod == ALMODE_KEEP_REL)
      {
         /* Keep same relative column */
         int orig_delta = pc->orig_col - prev->orig_col;
         if (orig_delta < min_delta)
         {
            orig_delta = min_delta;
         }
         pc->column = prev->column + orig_delta;
      }
      else /* ALMODE_SHIFT */
      {
         /* Shift by the same amount */
         pc->column += col_delta;
         if (pc->column < min_col)
         {
            pc->column = min_col;
         }
      }
      LOG_FMT(LINDLINED, "   %s set column of %s on line %d to col %d (orig %d)\n",
              (almod == ALMODE_KEEP_ABS) ? "abs" :
              (almod == ALMODE_KEEP_REL) ? "rel" : "sft",
              get_token_name(pc->type), pc->orig_line, pc->column, pc->orig_col);
   } while ((pc != NULL) && (pc->nl_count == 0));
}


/**
 * Changes the initial indent for a line to the given column
 *
 * @param pc      The chunk at the start of the line
 * @param column  The desired column
 */
void reindent_line2(chunk_t *pc, int column, const char *fcn_name, int lineno)
{
   LOG_FMT(LINDLINE, "%s: %d] col %d on %s [%s] => %d <called from '%s' line %d\n",
           __func__, pc->orig_line, pc->column, pc->str.c_str(),
           get_token_name(pc->type), column, fcn_name, lineno);

   if (column == pc->column)
   {
      return;
   }

   int col_delta = column - pc->column;
   int min_col   = column;

   pc->column = column;
   do
   {
      chunk_t *next = chunk_get_next(pc);

      if (next == NULL)
      {
         break;
      }
      min_col += space_col_align(pc, next);
      pc       = next;

      bool is_comment = chunk_is_comment(pc);
      bool keep       = is_comment && chunk_is_single_line_comment(pc) &&
                        cpd.settings[UO_indent_relative_single_line_comments].b;

      if (is_comment && (pc->parent_type != CT_COMMENT_EMBED) && !keep)
      {
         pc->column = pc->orig_col;
         if (pc->column < min_col)
         {
            pc->column = min_col; // + 1;
         }
         LOG_FMT(LINDLINE, "%s: set comment on line %d to col %d (orig %d)\n",
                 __func__, pc->orig_line, pc->column, pc->orig_col);
      }
      else
      {
         pc->column += col_delta;
         if (pc->column < min_col)
         {
            pc->column = min_col;
         }
         LOG_FMT(LINDLINED, "   set column of '%s' to %d (orig %d)\n",
                 pc->str.c_str(), pc->column, pc->orig_col);
      }
   } while ((pc != NULL) && (pc->nl_count == 0));
}
