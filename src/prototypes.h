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
void log_pcf_flags(log_sev_t sev, UINT64 flags);
const char *path_basename(const char *path);
int path_dirname_len(const char *filename);
const char *get_file_extension(int& idx);


/*
 *  output.cpp
 */

void output_parsed(FILE *pfile);


/*
 *  space.cpp
 */

void space_text(void);
void space_text_balance_nested_parens(void);
void space_add_after(chunk_t *pc, int count);


/*
 *  combine.cpp
 */

void fix_symbols(void);
void combine_labels(void);
void mark_comments(void);
void make_type(chunk_t *pc);

void flag_series(chunk_t *start, chunk_t *end, UINT64 set_flags, UINT64 clr_flags=0, chunk_nav_t nav = CNAV_ALL);

chunk_t *skip_template_next(chunk_t *ang_open);
chunk_t *skip_template_prev(chunk_t *ang_close);

chunk_t *skip_attribute_next(chunk_t *attr);
chunk_t *skip_attribute_prev(chunk_t *fp_close);


/*
 *  newlines.cpp
 */

#define newline_add_before(pc)    newline_add_before2(pc, __func__, __LINE__)
chunk_t *newline_add_before2(chunk_t *pc, const char *fcn, int line);

#define newline_force_before(pc)    newline_force_before2(pc, __func__, __LINE__)
chunk_t *newline_force_before2(chunk_t *pc, const char *fcn, int line);

#define newline_add_after(pc)     newline_add_after2(pc, __func__, __LINE__)
chunk_t *newline_add_after2(chunk_t *pc, const char *fcn, int line);

#define newline_force_after(pc)     newline_force_after2(pc, __func__, __LINE__)
chunk_t *newline_force_after2(chunk_t *pc, const char *fcn, int line);


/*
 *  tokenize.cpp
 */

void tokenize(const deque<int>& data, chunk_t *ref);


/*
 *  tokenize_cleanup.cpp
 */

void tokenize_cleanup(void);


/*
 *  brace_cleanup.cpp
 */

void brace_cleanup(void);


/*
 *  keywords.cpp
 */

int load_keyword_file(const char *filename);
c_token_t find_keyword_type(const char *word, int len);
void add_keyword(const char *tag, c_token_t type);
void print_keywords(FILE *pfile);
void clear_keyword_file(void);
pattern_class get_token_pattern_class(c_token_t tok);
bool keywords_are_sorted(void);


/*
 *  defines.cpp
 */

int load_define_file(const char *filename);
void add_define(const char *tag, const char *value);
void print_defines(FILE *pfile);
void clear_defines(void);


/*
 *  punctuators.cpp
 */
const chunk_tag_t *find_punctuator(const char *str, int lang_flags);


/*
 *  parse_frame.cpp
 */

void pf_copy(struct parse_frame *dst, const struct parse_frame *src);
void pf_push(struct parse_frame *pf);
void pf_push_under(struct parse_frame *pf);
void pf_copy_tos(struct parse_frame *pf);
void pf_trash_tos(void);
void pf_pop(struct parse_frame *pf);
int pf_check(struct parse_frame *frm, chunk_t *pc);


/*
 * lang_pawn.cpp
 */
void pawn_prescan(void);
void pawn_add_virtual_semicolons();
chunk_t *pawn_check_vsemicolon(chunk_t *pc);
chunk_t *pawn_add_vsemi_after(chunk_t *pc);


/*
 * unicode.cpp
 */
void write_bom(FILE *pf, CharEncoding enc);
void write_char(FILE *pf, int ch, CharEncoding enc);
void write_string(FILE *pf, const deque<int>& text, CharEncoding enc);
void write_string(FILE *pf, const char *ascii_text, CharEncoding enc);
bool decode_unicode(const vector<UINT8>& in_data, deque<int>& out_data, CharEncoding& enc, bool& has_bom);
void encode_utf8(int ch, vector<UINT8>& res);


/* Options we couldn't quite get rid of */
#define UO_input_tab_size 8
#define UO_indent_else_if false
#define UO_tok_split_gte false
#define UO_string_escape_char '\\'
#define UO_string_escape_char2 '0'

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
