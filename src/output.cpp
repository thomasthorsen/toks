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


typedef enum
{
   IT_UNKNOWN,
   IT_MACRO,             // preprocessor macro
   IT_MACRO_FUNCTION,    // function like preprocessor macro
   IT_FUNCTION,          // functions
   IT_STRUCT,            // struct <tag>
   IT_UNION,             // union <tag>
   IT_ENUM,              // enum <tag>
   IT_ENUM_VAL,          // values of an enum
   IT_CLASS,             // class
   IT_STRUCT_TYPE,       // typedef alias of a struct
   IT_UNION_TYPE,        // typedef alias of a union
   IT_ENUM_TYPE,         // typedef alias of an enum
   IT_FUNCTION_TYPE,     // typedef of a function or function ptr
   IT_TYPE,              // a type
   IT_VAR,               // a variable
   IT_NAMESPACE,         // a namespace
} id_type;

typedef enum
{
   IST_UNKNOWN,
   IST_DEFINITION,
   IST_DECLARATION,
   IST_REFERENCE,
} id_sub_type;

const char *type_strings[] =
{
   "UNKNOWN",
   "MACRO",
   "MACRO_FUNCTION",
   "FUNCTION",
   "STRUCT",
   "UNION",
   "ENUM",
   "ENUM_VAL",
   "CLASS",
   "STRUCT_TYPE",
   "UNION_TYPE",
   "ENUM_TYPE",
   "FUNCTION_TYPE",
   "TYPE",
   "VAR",
   "NAMESPACE",
};

const char *sub_type_strings[] =
{
   "UNKNOWN",
   "DEF",
   "DECL",
   "REF",
};


static id_sub_type sub_type_from_flags(chunk_t *pc)
{
   if (pc->flags & PCF_DEF)
      return IST_DEFINITION;
   else if (pc->flags & PCF_PROTO)
      return IST_DECLARATION;
   else if (pc->flags & PCF_REF)
      return IST_REFERENCE;
   else
      return IST_UNKNOWN;
}

void output_identifier(
   const char *file,
   unsigned int line,
   int column_start,
   int column_end,
   const char *scope,
   id_type type,
   id_sub_type sub_type,
   const char *identifier)
{
   printf("%s:%u", file, line);
   if (column_start != -1)
      printf(":%d", column_start);
   if (column_end != -1)
      printf(":%d", column_end);
   printf(" %s %s %s %s\n", scope, type_strings[type], sub_type_strings[sub_type], identifier);
}

void output(fp_data& fpd)
{
   chunk_t *pc;
   id_type type;
   id_sub_type sub_type;

   for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
   {
      if (pc->flags & PCF_PUNCTUATOR)
         continue;

      type = IT_UNKNOWN;
      sub_type = IST_UNKNOWN;

      switch (pc->type)
      {
         case CT_FUNC_DEF:
            type = IT_FUNCTION;
            sub_type = IST_DEFINITION;
            break;
         case CT_FUNC_PROTO:
            type = IT_FUNCTION;
            sub_type = IST_DECLARATION;
            break;
         case CT_FUNC_CALL:
            type = IT_FUNCTION;
            sub_type = IST_REFERENCE;
            break;
         case CT_FUNC_CLASS:
            type = IT_FUNCTION;
            sub_type = sub_type_from_flags(pc);
            break;
         case CT_MACRO_FUNC:
            type = IT_MACRO_FUNCTION;
            sub_type = IST_DEFINITION;
            break;
         case CT_MACRO:
            type = IT_MACRO;
            sub_type = IST_DEFINITION;
            break;
         case CT_TYPE:
         {
            if (pc->flags & PCF_KEYWORD)
               continue;

            if (pc->parent_type == CT_TYPEDEF)
            {
               if (pc->flags & PCF_TYPEDEF_STRUCT)
                  type = IT_STRUCT_TYPE;
               else if (pc->flags & PCF_TYPEDEF_UNION)
                  type = IT_UNION_TYPE;
               else if (pc->flags & PCF_TYPEDEF_ENUM)
                  type = IT_ENUM_TYPE;
               else
                  type = IT_TYPE;
               sub_type = IST_DEFINITION;
            }
            else if (pc->parent_type == CT_STRUCT ||
                     pc->parent_type == CT_UNION ||
                     pc->parent_type == CT_ENUM)
            {
               if (pc->parent_type == CT_STRUCT)
                  type = IT_STRUCT;
               else if (pc->parent_type == CT_UNION)
                  type = IT_UNION;
               else if (pc->parent_type == CT_ENUM)
                  type = IT_ENUM;
               sub_type = sub_type_from_flags(pc);
            }
            else if (pc->parent_type == CT_CLASS)
            {
               type = IT_CLASS;
               sub_type = sub_type_from_flags(pc);
            }
            else
            {
               type = IT_TYPE;
               sub_type = IST_REFERENCE;
            }
            break;
         }
         case CT_FUNC_TYPE:
            type = IT_FUNCTION_TYPE;
            sub_type = IST_DEFINITION;
            break;
         case CT_FUNC_CTOR_VAR:
         case CT_FUNC_VAR:
         case CT_WORD:
         {
            if (pc->parent_type == CT_NONE)
            {
               if (pc->flags & PCF_IN_ENUM)
               {
                  type = IT_ENUM_VAL;
                  sub_type = IST_DEFINITION;
               }
               else if (pc->flags & PCF_VAR_DEF)
               {
                  type = IT_VAR;
                  sub_type = IST_DEFINITION;
               }
               else if (pc->flags & PCF_VAR_DECL)
               {
                  type = IT_VAR;
                  sub_type = IST_DECLARATION;
               }
               else
               {
                  type = IT_VAR;
                  sub_type = IST_REFERENCE;
               }
            }
            else if (pc->parent_type == CT_NAMESPACE)
            {
               type = IT_NAMESPACE;
               sub_type = sub_type_from_flags(pc);
            }
            break;
         }
         default:
            continue;
      }

      output_identifier(fpd.filename,
                        pc->orig_line,
                        pc->orig_col,
                        pc->orig_col_end,
                        pc->scope,
                        type,
                        sub_type,
                        pc->str);
      (void) index_insert_entry(fpd,
                                pc->orig_line,
                                pc->orig_col,
                                pc->orig_col_end,
                                pc->scope,
                                type_strings[type],
                                sub_type_strings[sub_type],
                                pc->str);
   }
}

void output_dump_tokens()
{
   chunk_t *pc;
   int     cnt;
   const char *tolog;

   printf("# -=====-\n");
   printf("# Line      Tag          Parent     Scope     Columns  Br/Lvl/pp Flag Nl  Text");
   for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
   {
      printf("\n# %3d> %13.13s[%13.13s][%10.10s][%2d/%2d/%2d][%d/%d/%d][%d]",
              pc->orig_line, get_token_name(pc->type),
              get_token_name(pc->parent_type), pc->scope_text(),
              pc->column, pc->orig_col, pc->orig_col_end,
              pc->brace_level, pc->level, pc->pp_level,
              pc->nl_count);

      tolog = NULL;
      for (int i = 0; i < (int)ARRAY_SIZE(pcf_names); i++)
      {
       if ((pc->flags & (1ULL << i)) != 0)
       {
          if (tolog != NULL)
          {
             printf("%s", tolog);
             printf(",");
          }
          tolog = pcf_names[i];
       }
      }

      if (tolog != NULL)
      {
         printf("%s", tolog);
      }

      if ((pc->type != CT_NEWLINE) && (pc->len() != 0))
      {
         for (cnt = 0; cnt < pc->column; cnt++)
         {
            printf(" ");
         }
         if (pc->type != CT_NL_CONT)
         {
            printf("%s", pc->str.c_str());
         }
         else
         {
            printf("\\");
         }
      }
   }
   printf("\n# -=====-\n");
   fflush(stdout);
}
