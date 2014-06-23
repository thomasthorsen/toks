/**
 * @file toks.cpp
 * This file takes an input C/C++/D/Java file and reformats it.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#define DEFINE_CHAR_TABLE

#include "config.h"
#include "toks_types.h"
#include "char_table.h"
#include "chunk_list.h"
#include "prototypes.h"
#include "token_names.h"
#include "args.h"
#include "logger.h"
#include "log_levels.h"
#include "md5.h"
#include "sqlite3080200.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <cctype>
#include <strings.h>  /* strcasecmp() */
#include <vector>
#include <deque>

/* Global data */
struct cp_data cpd;


static int language_from_tag(const char *tag);
static int language_from_filename(const char *filename);
static const char *language_to_string(int lang);
static void toks_start(fp_data& fpd);
static void toks_end(fp_data& fpd);
static void do_source_file(const char *filename_in, bool dump);
static bool process_source_list(const char *source_list, deque<string>& source_files);


/**
 * Replace the brain-dead and non-portable basename().
 * Returns a pointer to the character after the last '/'.
 * The returned value always points into path, unless path is NULL.
 *
 * Input            Returns
 * NULL          => ""
 * "/some/path/" => ""
 * "/some/path"  => "path"
 * "afile"       => "afile"
 *
 * @param path The path to look at
 * @return     Pointer to the character after the last path seperator
 */
const char *path_basename(const char *path)
{
   if (path == NULL)
   {
      return("");
   }

   const char *last_path = path;
   char       ch;

   while ((ch = *path) != 0)
   {
      path++;
      /* Check both slash types to support windows */
      if ((ch == '/') || (ch == '\\'))
      {
         last_path = path;
      }
   }
   return(last_path);
}


/**
 * Returns the length of the directory part of the filename.
 */
int path_dirname_len(const char *filename)
{
   if (filename == NULL)
   {
      return(0);
   }
   return((int)(path_basename(filename) - filename));
}


static void usage_exit(const char *msg, const char *argv0, int code)
{
   if (msg != NULL)
   {
      fprintf(stderr, "%s\n", msg);
   }
   if ((code != EXIT_SUCCESS) || (argv0 == NULL))
   {
      fprintf(stderr, "Try running with -h for usage information\n");
      exit(code);
   }
   fprintf(stdout,
           "Usage:\n"
           "%s [options] [files ...]\n"
           "\n"
           "Basic Options:\n"
           " -F <file>     : Read files to process from file, one filename per line (- is stdin)\n"
           " -i <file>     : Use file as index (default: TOKS)\n"
           " -o <file>     : Redirect output to file\n"
           " -l <language> : Language override: C, CPP, D, CS, JAVA, PAWN, OC, OC+\n"
           " -t            : Load a file with types (usually not needed)\n"
           "\n"
           "Lookup Options (can be combined, supports ? and * wildcards):\n"
           " --id <name>          : Identifier name to search for\n"
           " --refs               : Show only references\n"
           " --defs               : Show only definitions\n"
           " --decls              : Show only declarations\n"
           "\n"
           "Config/Help Options:\n"
           " -h -? --help --usage     : print this message and exit\n"
           " --version                : print the version and exit\n"
           "\n"
           "Debug Options:\n"
           " -d            : Dump all tokens after parsing a file\n"
           " -L <severity> : Set the log severity (see log_levels.h)\n"
           " -s            : Show the log severity in the logs\n"
           "\n"
           "Usage Examples\n"
           "toks foo.d\n"
           "toks -L0-2,20-23,51 foo.d\n"
           "toks --id my_identifier\n"
           "\n"
           ,
           path_basename(argv0));
   exit(code);
}


static void version_exit(void)
{
   printf("toks %s\n", VERSION);
   exit(0);
}


static void redir_stdout(const char *output_file)
{
   /* Reopen stdout */
   FILE *my_stdout = stdout;

   if (output_file != NULL)
   {
      my_stdout = freopen(output_file, "wb", stdout);
      if (my_stdout == NULL)
      {
         LOG_FMT(LERR, "Unable to open %s for write: %s (%d)\n",
                 output_file, strerror(errno), errno);
         exit(EXIT_FAILURE);
      }
      LOG_FMT(LNOTE, "Redirecting output to %s\n", output_file);
   }
}

int main(int argc, char *argv[])
{
   const char *output_file, *source_list, *index_file;
   log_mask_t mask;
   int idx;
   const char *p_arg;
   bool dump = false;
   int retval;
   const char *identifier;
   bool refs, defs, decls;

   Args arg(argc, argv);

   if (arg.Present("--version") || arg.Present("-v"))
   {
      version_exit();
   }
   if (arg.Present("--help") || arg.Present("-h") ||
       arg.Present("--usage") || arg.Present("-?"))
   {
      usage_exit(NULL, argv[0], EXIT_SUCCESS);
   }

#ifdef WIN32
   /* tell windoze not to change what I write to stdout */
   (void)_setmode(_fileno(stdout), _O_BINARY);
#endif

   /* Init logging */
   log_init(stderr);
   if (((p_arg = arg.Param("-L")) != NULL) ||
       ((p_arg = arg.Param("--log")) != NULL))
   {
      logmask_from_string(p_arg, mask);
      log_set_mask(mask);
   }

   /* Enable log sevs? */
   if (arg.Present("-s") || arg.Present("--show"))
   {
      log_show_sev(true);
   }

   if (arg.Present("-d"))
   {
      dump = true;
   }

   /* Load type files */
   idx = 0;
   while ((p_arg = arg.Params("-t", idx)) != NULL)
   {
      load_keyword_file(p_arg);
   }

   /* Check for a language override */
   if ((p_arg = arg.Param("-l")) != NULL)
   {
      cpd.forced_lang_flags = language_from_tag(p_arg);
      if (cpd.forced_lang_flags == LANG_NONE)
      {
         LOG_FMT(LWARN, "Ignoring unknown language: %s\n", p_arg);
      }
   }

   source_list = arg.Param("-F");
   output_file = arg.Param("-o");
   index_file = arg.Param("-i");

   identifier = arg.Param("--id");

   refs = arg.Present("--refs");
   defs = arg.Present("--defs");
   decls = arg.Present("--decls");
   if (!(refs || defs || decls))
   {
      refs = defs = decls = true;
   }

   LOG_FMT(LNOTE, "output_file = %s\n", (output_file != NULL) ? output_file : "null");
   LOG_FMT(LNOTE, "source_list = %s\n", (source_list != NULL) ? source_list : "null");
   LOG_FMT(LNOTE, "index_file = %s\n", (index_file != NULL) ? index_file : "null");
   LOG_FMT(LNOTE, "identifier = %s\n", (identifier != NULL) ? identifier : "null");

   /*
    *  Done parsing args
    */

   redir_stdout(output_file);

   retval = sqlite3_open(index_file != NULL ? index_file : "TOKS", &cpd.index);
   if ((retval != SQLITE_OK) || (cpd.index == NULL))
   {
      LOG_FMT(LERR, "Unable to open index (%d)\n", retval);
      return EXIT_FAILURE;
   }

   if (!index_check())
   {
      sqlite3_close(cpd.index);
      return EXIT_FAILURE;
   }

   /* Check for unused args (ignore them) */
   idx   = 1;
   p_arg = arg.Unused(idx);

   if ((source_list != NULL) || (p_arg != NULL) || (identifier != NULL))
   {
      deque<string> source_files;

      if ((source_list != NULL) || (p_arg != NULL))
      {
         if (index_prepare_for_analysis() && index_prune_files())
         {
            /* Build a list of source files */
            if (p_arg != NULL)
            {
               idx = 1;
               while ((p_arg = arg.Unused(idx)) != NULL)
               {
                  source_files.push_back(p_arg);
               }
            }
            if (source_list != NULL)
            {
               (void) process_source_list(source_list, source_files);
            }

            size_t size = source_files.size();

            for (size_t i = 0; i < size; i += 1)
            {
               const char *fn = source_files.at(i).c_str();
               do_source_file(fn, dump);
            }

            index_end_analysis();
         }
      }

      if (identifier != NULL)
      {
         if (decls)
         {
            (void) index_lookup_identifier(identifier, IST_DECLARATION);
         }
         if (defs)
         {
            (void) index_lookup_identifier(identifier, IST_DEFINITION);
         }
         if (refs)
         {
            (void) index_lookup_identifier(identifier, IST_REFERENCE);
         }
      }
   }
   else
   {
      /* no input specified */
      sqlite3_close(cpd.index);
      usage_exit(NULL, argv[0], EXIT_SUCCESS);
   }

   clear_keyword_file();

   sqlite3_close(cpd.index);

   return EXIT_SUCCESS;
}


static bool process_source_list(const char *source_list, deque<string>& source_files)
{
   int from_stdin = strcmp(source_list, "-") == 0;
   FILE *p_file = from_stdin ? stdin : fopen(source_list, "r");

   if (p_file == NULL)
   {
      LOG_FMT(LERR, "%s: fopen(%s) failed: %s (%d)\n",
              __func__, source_list, strerror(errno), errno);
      return false;
   }

   char linebuf[256];
   char *fname;
   int  line = 0;
   int  len;

   while (fgets(linebuf, sizeof(linebuf), p_file) != NULL)
   {
      line++;
      fname = linebuf;
      len   = strlen(fname);
      while ((len > 0) && isspace((unsigned char) *fname))
      {
         fname++;
         len--;
      }
      while ((len > 0) && isspace((unsigned char) fname[len - 1]))
      {
         len--;
      }
      fname[len] = 0;
      while (len-- > 0)
      {
         if (fname[len] == '\\')
         {
            fname[len] = '/';
         }
      }

      LOG_FMT(LFILELIST, "%3d] %s\n", line, fname);

      if (fname[0] != '#')
      {
         source_files.push_back(fname);
      }
   }

   if (!from_stdin)
   {
      fclose(p_file);
   }

   return true;
}


/**
 * Does a source file.
 *
 * @param filename the file to read
 */
static void do_source_file(const char *filename, bool dump)
{
   fp_data fpd;

   fpd.filename = filename;
   fpd.frame_count = 0;
   fpd.frame_pp_level = 0;

   /* Do some simple language detection based on the filename extension */
   fpd.lang_flags = cpd.forced_lang_flags != LANG_NONE ?
      cpd.forced_lang_flags : language_from_filename(filename);

   /* Read in the source file */
   if (!decode_file(fpd.data, filename))
   {
      return;
   }

   /* Calculate MD5 digest */
   MD5::Calc(&fpd.data[0], fpd.data.size(), fpd.digest);

   if (index_prepare_for_file(fpd))
   {
      LOG_FMT(LNOTE, "Parsing: %s as language %s\n",
              filename, language_to_string(fpd.lang_flags));

      toks_start(fpd);

      /* Special hook for dumping parsed data for debugging */
      if (dump)
      {
         output_dump_tokens(fpd);
      }

      index_begin_file(fpd);

      output(fpd);

      index_end_file(fpd);

      toks_end(fpd);
   }
}


static void toks_start(fp_data& fpd)
{
   /**
    * Parse the text into chunks
    */
   tokenize(fpd);

   /**
    * Change certain token types based on simple sequence.
    * Example: change '[' + ']' to '[]'
    * Note that level info is not yet available, so it is OK to do all
    * processing that doesn't need to know level info. (that's very little!)
    */
   tokenize_cleanup(fpd);

   /**
    * Detect the brace and paren levels and insert virtual braces.
    * This handles all that nasty preprocessor stuff
    */
   brace_cleanup(fpd);

   /**
    * At this point, the level information is available and accurate.
    */

   if ((fpd.lang_flags & LANG_PAWN) != 0)
   {
      pawn_prescan(fpd);
   }

   /**
    * Re-type chunks, combine chunks
    */
   fix_symbols(fpd);

   /**
    * Look at all colons ':' and mark labels, :? sequences, etc.
    */
   combine_labels(fpd);

   /**
    * Assign scope information
    */
   assign_scope(fpd);
}


static void toks_end(fp_data& fpd)
{
   /* Free all the memory */
   chunk_t *pc;

   while ((pc = chunk_get_head(fpd)) != NULL)
   {
      chunk_del(fpd, pc);
   }
}


const char *get_token_name(c_token_t token)
{
   if ((token >= 0) && (token < (int)ARRAY_SIZE(token_names)) &&
       (token_names[token] != NULL))
   {
      return(token_names[token]);
   }
   return("???");
}


/**
 * Grab the token id for the text.
 * returns CT_NONE on failure to match
 */
c_token_t find_token_name(const char *text)
{
   int idx;

   if ((text != NULL) && (*text != 0))
   {
      for (idx = 1; idx < (int)ARRAY_SIZE(token_names); idx++)
      {
         if (strcasecmp(text, token_names[idx]) == 0)
         {
            return((c_token_t)idx);
         }
      }
   }
   return(CT_NONE);
}


static bool ends_with(const char *filename, const char *tag)
{
   int len1 = strlen(filename);
   int len2 = strlen(tag);

   if ((len2 <= len1) && (strcmp(&filename[len1 - len2], tag) == 0))
   {
      return(true);
   }
   return(false);
}


struct file_lang
{
   const char *ext;
   const char *tag;
   int        lang;
};

struct file_lang languages[] =
{
   { ".c",    "C",    LANG_C             },
   { ".cpp",  "CPP",  LANG_CPP           },
   { ".d",    "D",    LANG_D             },
   { ".cs",   "CS",   LANG_CS            },
   { ".vala", "VALA", LANG_VALA          },
   { ".java", "JAVA", LANG_JAVA          },
   { ".pawn", "PAWN", LANG_PAWN          },
   { ".p",    "",     LANG_PAWN          },
   { ".sma",  "",     LANG_PAWN          },
   { ".inl",  "",     LANG_PAWN          },
   { ".h",    "",     LANG_C             },
   { ".cxx",  "",     LANG_CPP           },
   { ".hpp",  "",     LANG_CPP           },
   { ".hxx",  "",     LANG_CPP           },
   { ".cc",   "",     LANG_CPP           },
   { ".cp",   "",     LANG_CPP           },
   { ".C",    "",     LANG_CPP           },
   { ".CPP",  "",     LANG_CPP           },
   { ".c++",  "",     LANG_CPP           },
   { ".di",   "",     LANG_D             },
   { ".m",    "OC",   LANG_OC            },
   { ".mm",   "OC+",  LANG_OC | LANG_CPP },
   { ".sqc",  "",     LANG_C             }, // embedded SQL
   { ".es",   "ECMA", LANG_ECMA          },
};

/**
 * Set idx = 0 before the first call.
 * Done when returns NULL
 */
const char *get_file_extension(int& idx)
{
   const char *val = NULL;

   if (idx < (int)ARRAY_SIZE(languages))
   {
      val = languages[idx].ext;
   }
   idx++;
   return(val);
}


/**
 * Find the language for the file extension
 * Default to C
 *
 * @param filename   The name of the file
 * @return           LANG_xxx
 */
static int language_from_filename(const char *filename)
{
   int i;

   for (i = 0; i < (int)ARRAY_SIZE(languages); i++)
   {
      if (ends_with(filename, languages[i].ext))
      {
         return(languages[i].lang);
      }
   }
   return(LANG_C);
}


/**
 * Find the language for the file extension
 *
 * @param filename   The name of the file
 * @return           LANG_xxx or LANG_NONE (no match)
 */
static int language_from_tag(const char *tag)
{
   int i;

   for (i = 0; i < (int)ARRAY_SIZE(languages); i++)
   {
      if (strcasecmp(tag, languages[i].tag) == 0)
      {
         return(languages[i].lang);
      }
   }
   return(LANG_NONE);
}


/**
 * Gets the tag text for a language
 *
 * @param lang    The LANG_xxx enum
 * @return        A string
 */
static const char *language_to_string(int lang)
{
   int i;

   /* Check for an exact match first */
   for (i = 0; i < (int)ARRAY_SIZE(languages); i++)
   {
      if (languages[i].lang == lang)
      {
         return(languages[i].tag);
      }
   }

   /* Check for the first set language bit */
   for (i = 0; i < (int)ARRAY_SIZE(languages); i++)
   {
      if ((languages[i].lang & lang) != 0)
      {
         return(languages[i].tag);
      }
   }
   return("???");
}
