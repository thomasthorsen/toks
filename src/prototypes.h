/**
 * @file prototypes.h
 * Big jumble of prototypes used in toks.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef C_PARSE_PROTOTYPES_H_INCLUDED
#define C_PARSE_PROTOTYPES_H_INCLUDED

#include "toks_types.h"
#include "chunk_list.h"

#include <string>
#include <deque>

/*
 *  toks.cpp
 */

const char *get_token_name(c_token_t token);
c_token_t find_token_name(const char *text);
const char *path_basename(const char *path);
int path_dirname_len(const char *filename);
const char *get_file_extension(int& idx);


/*
 *  output.cpp
 */

void output(fp_data& fpd);
void output_dump_tokens(fp_data& fpd);
void output_identifier(
   const char *filename,
   UINT32 line,
   UINT32 column_start,
   const char *scope,
   const char *type,
   const char *sub_type,
   const char *identifier);


/*
 *  combine.cpp
 */

void fix_symbols(fp_data& fpd);
void combine_labels(fp_data& fpd);
void mark_comments(fp_data& fpd);
void make_type(chunk_t *pc);

void flag_series(chunk_t *start, chunk_t *end, UINT64 set_flags, UINT64 clr_flags=0, chunk_nav_t nav = CNAV_ALL);

chunk_t *skip_template_next(chunk_t *ang_open);
chunk_t *skip_template_prev(chunk_t *ang_close);

chunk_t *skip_attribute_next(chunk_t *attr);
chunk_t *skip_attribute_prev(chunk_t *fp_close);


/*
 *  tokenize.cpp
 */

void tokenize(fp_data& fpd);


/*
 *  tokenize_cleanup.cpp
 */

void tokenize_cleanup(fp_data& fpd);


/*
 *  brace_cleanup.cpp
 */

void brace_cleanup(fp_data& fpd);


/*
 *  keywords.cpp
 */

bool load_keyword_file(const char *filename);
c_token_t find_keyword_type(const char *word, int len, c_token_t in_preproc, int lang_flags);
void add_keyword(const char *tag, c_token_t type);
void print_keywords(FILE *pfile);
void clear_keyword_file(void);
pattern_class get_token_pattern_class(c_token_t tok);
bool keywords_are_sorted(void);


/*
 *  punctuators.cpp
 */
const chunk_tag_t *find_punctuator(const char *str, int lang_flags);


/*
 *  parse_frame.cpp
 */
void pf_push(fp_data& fpd, struct parse_frame *pf);
void pf_push_under(fp_data& fpd, struct parse_frame *pf);
void pf_pop(fp_data& fpd, struct parse_frame *pf);
int pf_check(fp_data& fpd, struct parse_frame *frm, chunk_t *pc);


/*
 * lang_pawn.cpp
 */
void pawn_prescan(fp_data& fpd);
void pawn_add_virtual_semicolons(fp_data& fpd);
chunk_t *pawn_check_vsemicolon(fp_data& fpd, chunk_t *pc);
chunk_t *pawn_add_vsemi_after(fp_data& fpd, chunk_t *pc);


/*
 * unicode.cpp
 */
bool decode_file(vector<UINT8>& out_data, const char *filename);


/*
 * scope.cpp
 */
void assign_scope(fp_data& fpd);


/*
 * index.cpp
 */
bool index_check(void);
bool index_prepare_for_analysis(void);
void index_end_analysis(void);
bool index_prepare_for_file(fp_data& fpd);
void index_begin_file(fp_data& fpd);
void index_end_file(fp_data& fpd);
bool index_insert_entry(
   fp_data& fpd,
   UINT32 line,
   UINT32 column_start,
   const char *scope,
   const char *type,
   const char *sub_type,
   const char *identifier);
bool index_lookup_identifier(
   const char *identifier,
   const char *type,
   const char *sub_type);


/* Options we couldn't quite get rid of */
#define UO_input_tab_size 8
#define UO_indent_else_if false
#define UO_tok_split_gte false
#define UO_string_escape_char '\\'
#define UO_string_escape_char2 0

/**
 * Advances to the next tab stop.
 * Column 1 is the left-most column.
 *
 * @param col     The current column
 * @param tabsize The tabsize
 * @return the next tabstop column
 */
static_inline
int calc_next_tab_column(int col, int tabsize)
{
   if (col <= 0)
   {
      col = 1;
   }
   col = 1 + ((((col - 1) / tabsize) + 1) * tabsize);
   return(col);
}

#endif   /* C_PARSE_PROTOTYPES_H_INCLUDED */
