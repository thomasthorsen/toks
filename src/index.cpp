
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

bool index_check(void)
{
   int result;
   int version = 0;

   result = sqlite3_exec(
     cpd.index,
     "SELECT Version FROM Version",
     version_check_callback,
     &version,
     NULL);

   if (result == SQLITE_OK)
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

      result = sqlite3_exec(
         cpd.index,
         "CREATE TABLE Version(Version INTEGER)",
         NULL,
         NULL,
         &errmsg);

      if (result == SQLITE_OK)
      {
         result = sqlite3_exec(
            cpd.index,
            "INSERT INTO Version VALUES(" xstr(INDEX_VERSION) ")",
            NULL,
            NULL,
            &errmsg);
      }

      if (result == SQLITE_OK)
      {
         result = sqlite3_exec(
            cpd.index,
            "CREATE TABLE Files(Digest TEXT, Filename TEXT PRIMARY KEY)",
            NULL,
            NULL,
            &errmsg);
      }

      if (result == SQLITE_OK)
      {
         result = sqlite3_exec(
            cpd.index,
            "CREATE TABLE Entries(Digest TEXT, Line INTEGER, ColumnStart INTEGER, ColumnEnd INTEGER, Context TEXT, Type TEXT, SubType TEXT, Identifier TEXT)",
            NULL,
            NULL,
            &errmsg);
      }

      if (result != SQLITE_OK)
      {
         LOG_FMT(LERR, "Index access error (%d: %s)\n", result, errmsg != NULL ? errmsg : "");
         return false;
      }
   }
   return true;
}

static int insert_file(const char digest[33], const char *filename)
{
   sqlite3_stmt *stmt_insert_file = NULL;
   int result;

   result = sqlite3_prepare_v2(cpd.index,
                               "INSERT INTO Files VALUES(?,?)",
                               -1,
                               &stmt_insert_file,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_insert_file,
                                 1,
                                 digest,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_insert_file,
                                 2,
                                 filename,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(stmt_insert_file);
   }

   (void) sqlite3_finalize(stmt_insert_file);

   return result;
}

static int prune_entries(const char digest[33])
{
   sqlite3_stmt *stmt_prune_entries = NULL;
   int result;

   result = sqlite3_prepare_v2(cpd.index,
                               "DELETE FROM Entries WHERE Digest=?",
                               -1,
                               &stmt_prune_entries,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_prune_entries,
                                 1,
                                 digest,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(stmt_prune_entries);
   }

   (void) sqlite3_finalize(stmt_prune_entries);

   return result;
}

static int replace_file(const char digest[33], const char *filename)
{
   sqlite3_stmt *stmt_change_digest = NULL;
   int result;

   result = sqlite3_prepare_v2(cpd.index,
                               "UPDATE Files SET Digest=? WHERE Filename=?",
                               -1,
                               &stmt_change_digest,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_change_digest,
                                 1,
                                 digest,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_change_digest,
                                 2,
                                 filename,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(stmt_change_digest);
   }

   (void) sqlite3_finalize(stmt_change_digest);

   return result;
}

bool index_prepare_for_file(const char digest[33], const char *filename)
{
   sqlite3_stmt *stmt_lookup_file = NULL;
   int result;
   bool retval = true;

   result = sqlite3_prepare_v2(cpd.index,
                               "SELECT Digest FROM Files WHERE Filename=?",
                               -1,
                               &stmt_lookup_file,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_lookup_file,
                                 1,
                                 filename,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(stmt_lookup_file);
   }

   if (result == SQLITE_ROW)
   {
      const char *ingest = (const char *) sqlite3_column_text(stmt_lookup_file, 0);
      if (strcmp(digest, ingest) == 0)
      {
         LOG_FMT(LNOTE, "File %s(%s) exists in index with same digest\n", filename, digest);
         result = SQLITE_DONE;
         retval = false;
      }
      else
      {
         LOG_FMT(LNOTE, "File %s(%s) exists in index with different digest (%s)\n", filename, digest, ingest);
         result = replace_file(digest, filename);
         if (result == SQLITE_DONE)
         {
            result = prune_entries(ingest);
         }
      }
   }
   else
   {
      LOG_FMT(LNOTE, "File %s(%s) does not exist in index\n", filename, digest);
      result = insert_file(digest, filename);
   }

   if (result != SQLITE_DONE)
   {
      const char *errmsg = sqlite3_errmsg(cpd.index);
      LOG_FMT(LERR, "Index access error (%d: %s)\n", result, errmsg != NULL ? errmsg : "");
      retval = false;
   }

   (void) sqlite3_finalize(stmt_lookup_file);

   return retval;
 }
