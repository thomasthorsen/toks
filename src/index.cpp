
#include <cstdio>
#include <cstdlib>

#include "toks_types.h"
#include "sqlite3080001.h"

#define INDEX_VERSION 1

#define xstr(a) str(a)
#define str(a) #a

static int index_version_check_callback(
   void *version,
   int argc,
   char **argv,
   char **azColName)
{
   if ((argc == 1) && (argv[0] != NULL))
      *((int *) version) = atoi(argv[0]);
   return 0;
}

bool index_check(void)
{
   int result;
   int version = 0;
   bool retval = true;

   result = sqlite3_exec(
     cpd.index,
     "SELECT Version FROM Version",
     index_version_check_callback,
     &version,
     NULL);

   if (result == SQLITE_OK)
   {
      if (version != INDEX_VERSION)
      {
         LOG_FMT(LERR, "Wrong index format version, delete it to continue\n");
         retval = false;
      }
   }
   else
   {
      char *errmsg = NULL;

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
            "CREATE TABLE Files(Digest TEXT, Filename TEXT UNIQUE)",
            NULL,
            NULL,
            &errmsg);
      }

      if (result == SQLITE_OK)
      {
         result = sqlite3_exec(
            cpd.index,
            "CREATE TABLE Entries(Filerow INTEGER, Line INTEGER, ColumnStart INTEGER, ColumnEnd INTEGER, Context TEXT, Type TEXT, SubType TEXT, Identifier TEXT)",
            NULL,
            NULL,
            &errmsg);
      }

      if (result != SQLITE_OK)
      {
         LOG_FMT(LERR, "Index access error (%d: %s)\n", result, errmsg != NULL ? errmsg : "");
         retval = false;
      }

      sqlite3_free(errmsg);
   }

   return retval;
}

static int index_lookup_filerow(const char *filename, sqlite3_int64 *filerow)
{
   sqlite3_stmt *stmt_lookup_filerow = NULL;
   int result;

   result = sqlite3_prepare_v2(cpd.index,
                               "SELECT rowid FROM Files WHERE Filename=?",
                               -1,
                               &stmt_lookup_filerow,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_lookup_filerow,
                                 1,
                                 filename,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(stmt_lookup_filerow);
      if (result == SQLITE_ROW)
      {
         *filerow = sqlite3_column_int64(stmt_lookup_filerow, 0);
         result = SQLITE_OK;
      }
   }

   (void) sqlite3_finalize(stmt_lookup_filerow);

   return result;
}

static int index_insert_file(
   const char *digest,
   const char *filename)
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
      if (result == SQLITE_DONE)
      {
         result = SQLITE_OK;
      }
   }

   (void) sqlite3_finalize(stmt_insert_file);

   return result;
}

static int index_prune_entries(sqlite3_int64 filerow)
{
   sqlite3_stmt *stmt_prune_entries = NULL;
   int result;

   result = sqlite3_prepare_v2(cpd.index,
                               "DELETE FROM Entries WHERE Filerow=?",
                               -1,
                               &stmt_prune_entries,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_int64(stmt_prune_entries,
                                  1,
                                  filerow);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(stmt_prune_entries);
      if (result == SQLITE_DONE)
      {
         result = SQLITE_OK;
      }
   }

   (void) sqlite3_finalize(stmt_prune_entries);

   return result;
}

static int index_replace_file(const char *digest, const char *filename)
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
      if (result == SQLITE_DONE)
      {
         result = SQLITE_OK;
      }
   }

   (void) sqlite3_finalize(stmt_change_digest);

   return result;
}

/* Returns true if the file needs to be analyzed */
bool index_prepare_for_file(
   const char *digest,
   const char *filename,
   sqlite3_int64 *filerow)
{
   sqlite3_stmt *stmt_lookup_file = NULL;
   int result;
   bool retval = true;

   result = sqlite3_prepare_v2(cpd.index,
                               "SELECT rowid,Digest FROM Files WHERE Filename=?",
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
      *filerow = sqlite3_column_int64(stmt_lookup_file, 0);
      const char *ingest =
         (const char *) sqlite3_column_text(stmt_lookup_file, 1);

      if (strcmp(digest, ingest) == 0)
      {
         LOG_FMT(LNOTE, "File %s(%s) exists in index at filerow %lld with same digest\n", filename, digest, *filerow);
         result = SQLITE_OK;
         retval = false;
      }
      else
      {
         LOG_FMT(LNOTE, "File %s(%s) exists in index at filerow %lld with different digest (%s)\n", filename, digest, *filerow, ingest);
         result = index_replace_file(digest, filename);
         if (result == SQLITE_OK)
         {
            result = index_prune_entries(*filerow);
         }
      }
   }
   else
   {
      LOG_FMT(LNOTE, "File %s(%s) does not exist in index\n", filename, digest);
      result = index_insert_file(digest, filename);
      if (result == SQLITE_OK)
      {
         result = index_lookup_filerow(filename, filerow);
      }
   }

   if (result != SQLITE_OK)
   {
      const char *errstr = sqlite3_errstr(result);
      LOG_FMT(LERR, "Index access error (%d: %s)\n", result, errstr != NULL ? errstr : "");
      cpd.error_count++;
      retval = false;
   }

   (void) sqlite3_finalize(stmt_lookup_file);

   return retval;
}
