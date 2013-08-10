/**
 * @file toks.cpp
 * This file takes an input C/C++/D/Java file and reformats it.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#define DEFINE_PCF_NAMES
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include "unc_ctype.h"
#include <sys/stat.h>
#include <strings.h>  /* strcasecmp() */
#include <vector>
#include <deque>

/* Global data */
struct cp_data cpd;


static int language_from_tag(const char *tag);
static int language_from_filename(const char *filename);
static const char *language_to_string(int lang);
static bool read_stdin(file_mem& fm);
static void toks_start(const deque<int>& data);
static void toks_end();
static void toks_file(const file_mem& fm);
static void do_source_file(const char *filename_in);
static void process_source_list(const char *source_list);

static int load_mem_file(const char *filename, file_mem& fm);


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
           "If no input files are specified, the input is read from stdin\n"
           "If reading from stdin, you should specify the language using -l\n"
           "\n"
           "The output is dumped to stdout, unless redirected with -o FILE.\n"
           "\n"
           "Errors are always dumped to stderr\n"
           "\n"
           "Basic Options:\n"
           " -o FILE      : Redirect stdout to FILE\n"
           " -F FILE      : read files to process from FILE, one filename per line (- is stdin)\n"
           " files        : files to process (can be combined with -F)\n"
           " -l           : language override: C, CPP, D, CS, JAVA, PAWN, OC, OC+\n"
           " -t           : load a file with types (usually not needed)\n"
           "\n"
           "Config/Help Options:\n"
           " -h -? --help --usage     : print this message and exit\n"
           " --version                : print the version and exit\n"
           "\n"
           "Debug Options:\n"
           " -L SEV       : Set the log severity (see log_levels.h)\n"
           " -s           : Show the log severity in the logs\n"
           " --decode     : decode remaining args (chunk flags) and exit\n"
           "\n"
           "Usage Examples\n"
           "cat foo.d | toks -l d\n"
           "toks foo.d\n"
           "toks -L0-2,20-23,51 foo.d\n"
           "toks -o foo.out foo.d\n"
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
         usage_exit(NULL, NULL, 56);
      }
      LOG_FMT(LNOTE, "Redirecting output to %s\n", output_file);
   }
}


int main(int argc, char *argv[])
{
   const char *output_file = NULL;
   const char *source_list = NULL;
   log_mask_t mask;
   int        idx;
   const char *p_arg;

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
   }
   else
   {
      logmask_from_string("", mask);
   }
   log_set_mask(mask);

   if (arg.Present("--decode"))
   {
      logmask_from_string("0", mask);
      log_set_mask(mask);
      idx = 1;
      while ((p_arg = arg.Unused(idx)) != NULL)
      {
         log_pcf_flags(LSYS, strtoul(p_arg, NULL, 16));
      }
      return EXIT_SUCCESS;
   }

   /* Enable log sevs? */
   if (arg.Present("-s") || arg.Present("--show"))
   {
      log_show_sev(true);
   }

   /* Load type files */
   idx = 0;
   while ((p_arg = arg.Params("-t", idx)) != NULL)
   {
      load_keyword_file(p_arg);
   }

   /* add types */
   idx = 0;
   while ((p_arg = arg.Params("--type", idx)) != NULL)
   {
      add_keyword(p_arg, CT_TYPE);
   }

   /* Load define files */
   idx = 0;
   while ((p_arg = arg.Params("-d", idx)) != NULL)
   {
      load_define_file(p_arg);
   }

   /* add defines */
   idx = 0;
   while ((p_arg = arg.Params("--define", idx)) != NULL)
   {
      add_define(p_arg, NULL);
   }

   /* Check for a language override */
   if ((p_arg = arg.Param("-l")) != NULL)
   {
      cpd.lang_flags = language_from_tag(p_arg);
      if (cpd.lang_flags == 0)
      {
         LOG_FMT(LWARN, "Ignoring unknown language: %s\n", p_arg);
      }
      else
      {
         cpd.lang_forced = true;
      }
   }

   if (((source_list = arg.Param("--files")) == NULL) &&
       ((source_list = arg.Param("-F")) == NULL))
   {
      // not using a file list, source_list is NULL
   }

   /* Grab the output override */
   output_file = arg.Param("-o");

   LOG_FMT(LDATA, "output_file = %s\n", (output_file != NULL) ? output_file : "null");
   LOG_FMT(LDATA, "source_list = %s\n", (source_list != NULL) ? source_list : "null");

   /*
    *  Done parsing args
    */

   redir_stdout(output_file);

   /* Check for unused args (ignore them) */
   idx   = 1;
   p_arg = arg.Unused(idx);

   if ((source_list == NULL) && (p_arg == NULL))
   {
      /* no input specified, so use stdin */
      if (cpd.lang_flags == 0)
      {
         cpd.lang_flags = LANG_C;
      }

      file_mem fm;
      if (!read_stdin(fm))
      {
         LOG_FMT(LERR, "Failed to read stdin\n");
         return(100);
      }

      cpd.filename = "stdin";

      /* Done reading from stdin */
      LOG_FMT(LSYS, "Parsing: %d bytes (%d chars) from stdin as language %s\n",
              (int)fm.raw.size(), (int)fm.data.size(),
              language_to_string(cpd.lang_flags));

      toks_file(fm);
   }
   else
   {
      /* Doing multiple files */

      /* Do the files on the command line first */
      idx = 1;
      while ((p_arg = arg.Unused(idx)) != NULL)
      {
         do_source_file(p_arg);
      }

      if (source_list != NULL)
      {
         process_source_list(source_list);
      }
   }

   clear_keyword_file();
   clear_defines();

   return((cpd.error_count != 0) ? EXIT_FAILURE : EXIT_SUCCESS);
}


static void process_source_list(const char *source_list)
{
   int from_stdin = strcmp(source_list, "-") == 0;
   FILE *p_file = from_stdin ? stdin : fopen(source_list, "r");

   if (p_file == NULL)
   {
      LOG_FMT(LERR, "%s: fopen(%s) failed: %s (%d)\n",
              __func__, source_list, strerror(errno), errno);
      cpd.error_count++;
      return;
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
      while ((len > 0) && unc_isspace(*fname))
      {
         fname++;
         len--;
      }
      while ((len > 0) && unc_isspace(fname[len - 1]))
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
         do_source_file(fname);
      }
   }

   if (!from_stdin)
   {
      fclose(p_file);
   }
}


static bool read_stdin(file_mem& fm)
{
   deque<UINT8> dq;
   char         buf[4096];
   int          len;
   int          idx;

   fm.raw.clear();
   fm.data.clear();
   fm.enc = ENC_ASCII;

   while (!feof(stdin))
   {
      len = fread(buf, 1, sizeof(buf), stdin);
      for (idx = 0; idx < len; idx++)
      {
         dq.push_back(buf[idx]);
      }
   }

   /* Copy the raw data from the deque to the vector */
   fm.raw.insert(fm.raw.end(), dq.begin(), dq.end());
   return(decode_unicode(fm.raw, fm.data, fm.enc, fm.bom));
}


/**
 * Loads a file into memory
 */
static int load_mem_file(const char *filename, file_mem& fm)
{
   int         retval = -1;
   struct stat my_stat;
   FILE        *p_file;

   fm.raw.clear();
   fm.data.clear();
   fm.enc = ENC_ASCII;

   /* Grab the stat info for the file */
   if (stat(filename, &my_stat) < 0)
   {
      return(-1);
   }

   /* Try to read in the file */
   p_file = fopen(filename, "rb");
   if (p_file == NULL)
   {
      return(-1);
   }

   fm.raw.resize(my_stat.st_size);
   if (my_stat.st_size == 0)
   {
      /* Empty file */
      retval = 0;
      fm.bom = false;
      fm.enc = ENC_ASCII;
      fm.data.clear();
   }
   else
   {
      /* read the raw data */
      if (fread(&fm.raw[0], fm.raw.size(), 1, p_file) != 1)
      {
         LOG_FMT(LERR, "%s: fread(%s) failed: %s (%d)\n",
                 __func__, filename, strerror(errno), errno);
         cpd.error_count++;
      }
      else if (!decode_unicode(fm.raw, fm.data, fm.enc, fm.bom))
      {
         LOG_FMT(LERR, "%s: failed to decode the file '%s'\n", __func__, filename);
      }
      else
      {
         LOG_FMT(LNOTE, "%s: '%s' encoding looks like %d\n", __func__, filename, fm.enc);
         retval = 0;
      }
   }
   fclose(p_file);
   return(retval);
}


/**
 * Does a source file.
 *
 * @param filename_in  the file to read
 */
static void do_source_file(const char *filename_in)
{
   file_mem fm;
   string   filename_tmp;

   /* Do some simple language detection based on the filename extension */
   if (!cpd.lang_forced || (cpd.lang_flags == 0))
   {
      cpd.lang_flags = language_from_filename(filename_in);
   }

   /* Try to read in the source file */
   if (load_mem_file(filename_in, fm) < 0)
   {
      LOG_FMT(LERR, "Failed to load (%s)\n", filename_in);
      cpd.error_count++;
      return;
   }

   LOG_FMT(LSYS, "Parsing: %s as language %s\n",
           filename_in, language_to_string(cpd.lang_flags));

   cpd.filename = filename_in;
   toks_file(fm);
}


static void toks_start(const deque<int>& data)
{
   /**
    * Parse the text into chunks
    */
   tokenize(data, NULL);

   /**
    * Change certain token types based on simple sequence.
    * Example: change '[' + ']' to '[]'
    * Note that level info is not yet available, so it is OK to do all
    * processing that doesn't need to know level info. (that's very little!)
    */
   tokenize_cleanup();

   /**
    * Detect the brace and paren levels and insert virtual braces.
    * This handles all that nasty preprocessor stuff
    */
   brace_cleanup();

   /**
    * At this point, the level information is available and accurate.
    */

   if ((cpd.lang_flags & LANG_PAWN) != 0)
   {
      pawn_prescan();
   }

   /**
    * Re-type chunks, combine chunks
    */
   fix_symbols();

   mark_comments();

   /**
    * Look at all colons ':' and mark labels, :? sequences, etc.
    */
   combine_labels();
}


static void toks_file(const file_mem& fm)
{
   const deque<int>& data = fm.data;

   /* Check for embedded 0's (represents a decoding failure or corrupt file) */
   for (int idx = 0; idx < (int)data.size() - 1; idx++)
   {
      if (data[idx] == 0)
      {
         LOG_FMT(LERR, "An embedded 0 was found in '%s'.\n", cpd.filename);
         LOG_FMT(LERR, "The file may be encoded in an unsupported Unicode format.\n");
         LOG_FMT(LERR, "Aborting.\n");
         cpd.error_count++;
         return;
      }
   }

   toks_start(data);

   /* Special hook for dumping parsed data for debugging */
   /* output_parsed(stdout); */

   output();

   toks_end();
}


static void toks_end()
{
   /* Free all the memory */
   chunk_t *pc;

   while ((pc = chunk_get_head()) != NULL)
   {
      chunk_del(pc);
   }

   /* Clean up some state variables */
   cpd.al_cnt      = 0;
   cpd.did_newline = true;
   cpd.frame_count = 0;
   cpd.pp_level    = 0;
   cpd.changes     = 0;
   cpd.in_preproc  = CT_NONE;
   cpd.consumed    = false;
   cpd.preproc_ncnl_count = 0;
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
   { ".h",    "",     LANG_CPP           },
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
 * @return           LANG_xxx or 0 (no match)
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
   return(0);
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


void log_pcf_flags(log_sev_t sev, UINT64 flags)
{
   if (!log_sev_on(sev))
   {
      return;
   }

   log_fmt(sev, "[0x%" PRIx64 ":", flags);

   const char *tolog = NULL;
   for (int i = 0; i < (int)ARRAY_SIZE(pcf_names); i++)
   {
      if ((flags & (1ULL << i)) != 0)
      {
         if (tolog != NULL)
         {
            log_str(sev, tolog, strlen(tolog));
            log_str(sev, ",", 1);
         }
         tolog = pcf_names[i];
      }
   }

   if (tolog != NULL)
   {
      log_str(sev, tolog, strlen(tolog));
   }

   log_str(sev, "]\n", 2);
}
