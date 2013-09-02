
#include <cstdio>
#include <cstdlib>

#include "toks_types.h"
#include "sqlite3080001.h"

#define INDEX_VERSION 1

#define xstr(a) str(a)
#define str(a) #a

static int version_check_callback(void *version, int argc, char **argv, char **azColName)
{
   if ((argc == 1) && (argv[0] != NULL))
      *((int *) version) = atoi(argv[0]);
   return 0;
}

bool version_check(void)
{
   int retval;
   int version = 0;

   retval = sqlite3_exec(
     cpd.index,
     "SELECT Version FROM Version",
     version_check_callback,
     &version,
     NULL);

   if (retval == SQLITE_OK)
   {
      if (version != INDEX_VERSION)
      {
         LOG_FMT(LERR, "Wrong index format version, delete it to continue\n");
         return false;
      }
   }
   else
   {
      char *errmsg;

      retval = sqlite3_exec(
         cpd.index,
         "CREATE TABLE Version(Version INTEGER)",
         NULL,
         NULL,
         &errmsg);

      if (retval == SQLITE_OK)
      {
         retval = sqlite3_exec(
            cpd.index,
            "INSERT INTO Version VALUES (" xstr(INDEX_VERSION) ")",
            NULL,
            NULL,
            &errmsg);
      }

      if (retval != SQLITE_OK)
      {
         LOG_FMT(LERR, "Index access error (%d: %s)", retval, errmsg != NULL ? errmsg : "");
         return false;
      }
   }
   return true;
}
