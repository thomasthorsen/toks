
#include <cstdio>
#include <cstdlib>
#include <cinttypes>

#include "prototypes.h"
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
         "CREATE TABLE Version(Version INTEGER);"
         "INSERT INTO Version VALUES(" xstr(INDEX_VERSION) ");"
         "CREATE TABLE Files(Digest TEXT, Filename TEXT UNIQUE);"
         "CREATE TABLE Entries(Filerow INTEGER, Line INTEGER, ColumnStart INTEGER, ColumnEnd INTEGER, Scope TEXT, Type TEXT, SubType TEXT, Identifier TEXT);"
         "PRAGMA journal_mode=OFF;"
         "PRAGMA synchronous=OFF;",
         NULL,
         NULL,
         &errmsg);

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
bool index_prepare_for_file(fp_data& fpd)
{
   sqlite3_stmt *stmt_lookup_file = NULL;
   int result;
   bool retval = true;
   sqlite3_int64 filerow = 0;

   fpd.stmt_insert_entry = NULL;
   fpd.stmt_begin = NULL;
   fpd.stmt_commit = NULL;

   result = sqlite3_prepare_v2(cpd.index,
                               "SELECT rowid,Digest FROM Files WHERE Filename=?",
                               -1,
                               &stmt_lookup_file,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_lookup_file,
                                 1,
                                 fpd.filename,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(stmt_lookup_file);
   }

   if (result == SQLITE_ROW)
   {
      filerow = sqlite3_column_int64(stmt_lookup_file, 0);
      const char *ingest =
         (const char *) sqlite3_column_text(stmt_lookup_file, 1);

      if (strcmp(fpd.digest, ingest) == 0)
      {
         LOG_FMT(LNOTE, "File %s(%s) exists in index at filerow %"PRId64" with same digest\n", fpd.filename, fpd.digest, (int64_t) filerow);
         result = SQLITE_OK;
         retval = false;
      }
      else
      {
         LOG_FMT(LNOTE, "File %s(%s) exists in index at filerow %"PRId64" with different digest (%s)\n", fpd.filename, fpd.digest, (int64_t) filerow, ingest);
         result = index_replace_file(fpd.digest, fpd.filename);
         if (result == SQLITE_OK)
         {
            result = index_prune_entries(filerow);
         }
      }
   }
   else if (result == SQLITE_DONE)
   {
      result = index_insert_file(fpd.digest, fpd.filename);
      if (result == SQLITE_OK)
      {
         result = index_lookup_filerow(fpd.filename, &filerow);
      }
      LOG_FMT(LNOTE, "File %s(%s) does not exist in index, inserted at filerow %"PRId64"\n", fpd.filename, fpd.digest, (int64_t) filerow);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "INSERT INTO Entries VALUES(?,?,?,?,?,?,?,?)",
                                  -1,
                                  &fpd.stmt_insert_entry,
                                  NULL);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_int64(fpd.stmt_insert_entry,
                                  1,
                                  filerow);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "BEGIN",
                                  -1,
                                  &fpd.stmt_begin,
                                  NULL);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "COMMIT",
                                  -1,
                                  &fpd.stmt_commit,
                                  NULL);
   }

   if (result != SQLITE_OK)
   {
      const char *errstr = sqlite3_errstr(result);
      LOG_FMT(LERR, "Index access error (%d: %s)\n", result, errstr != NULL ? errstr : "");
      retval = false;
   }

   (void) sqlite3_finalize(stmt_lookup_file);

   return retval;
}

void index_begin_file(fp_data& fpd)
{
   (void) sqlite3_step(fpd.stmt_begin);
}

void index_end_file(fp_data& fpd)
{
   (void) sqlite3_step(fpd.stmt_commit);
   sqlite3_finalize(fpd.stmt_insert_entry);
   sqlite3_finalize(fpd.stmt_begin);
   sqlite3_finalize(fpd.stmt_commit);
}

bool index_insert_entry(
   fp_data& fpd,
   UINT32 line,
   UINT32 column_start,
   UINT32 column_end,
   const char *scope,
   const char *type,
   const char *sub_type,
   const char *identifier)
{
   bool retval = true;
   int result;

   result = sqlite3_reset(fpd.stmt_insert_entry);

   if (result == SQLITE_OK)
   {
      result |= sqlite3_bind_int64(fpd.stmt_insert_entry,
                                   2,
                                   line);
      result |= sqlite3_bind_int64(fpd.stmt_insert_entry,
                                   3,
                                   column_start);
      result |= sqlite3_bind_int64(fpd.stmt_insert_entry,
                                   4,
                                   column_end);
      result |= sqlite3_bind_text(fpd.stmt_insert_entry,
                                  5,
                                  scope,
                                  -1,
                                  SQLITE_STATIC);
      result |= sqlite3_bind_text(fpd.stmt_insert_entry,
                                  6,
                                  type,
                                  -1,
                                  SQLITE_STATIC);
      result |= sqlite3_bind_text(fpd.stmt_insert_entry,
                                  7,
                                  sub_type,
                                  -1,
                                  SQLITE_STATIC);
      result |= sqlite3_bind_text(fpd.stmt_insert_entry,
                                  8,
                                  identifier,
                                  -1,
                                  SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(fpd.stmt_insert_entry);
      if (result == SQLITE_DONE)
      {
         result = SQLITE_OK;
      }
   }

   if (result != SQLITE_OK)
   {
      const char *errstr = sqlite3_errstr(result);
      LOG_FMT(LERR, "Index access error (%d: %s)\n", result, errstr != NULL ? errstr : "");
      retval = false;
   }

   return retval;
}

bool index_lookup_identifier(const char *identifier, const char *type, const char *sub_type)
{
   bool retval = true;
   sqlite3_stmt *stmt_lookup_identifier;
   int result;

   result = sqlite3_prepare_v2(cpd.index,
                               "SELECT Files.Filename,Entries.Line,Entries.ColumnStart,Entries.ColumnEnd,Entries.Scope,Entries.Type,Entries.SubType,Entries.Identifier "
                               "FROM Files JOIN Entries ON Files.rowid=Entries.Filerow "
                               "WHERE Entries.Identifier LIKE ? "
                               "AND Entries.Type LIKE ? "
                               "AND Entries.SubType LIKE ?",
                               -1,
                               &stmt_lookup_identifier,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(stmt_lookup_identifier,
                                 1,
                                 identifier != NULL ? identifier : "%",
                                 -1,
                                 SQLITE_STATIC);
      result = sqlite3_bind_text(stmt_lookup_identifier,
                                 2,
                                 type != NULL ? type : "%",
                                 -1,
                                 SQLITE_STATIC);
      result = sqlite3_bind_text(stmt_lookup_identifier,
                                 3,
                                 sub_type != NULL ? sub_type : "%",
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      do
      {
         result = sqlite3_step(stmt_lookup_identifier);
         if (result == SQLITE_ROW)
         {
            const char *filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 0));
            UINT32 line = (UINT32) sqlite3_column_int64(stmt_lookup_identifier, 1);
            UINT32 column_start = (UINT32) sqlite3_column_int64(stmt_lookup_identifier, 2);
            UINT32 column_end = (UINT32) sqlite3_column_int64(stmt_lookup_identifier, 3);
            const char *scope = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 4));
            const char *type = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 5));
            const char *sub_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 6));
            const char *identifier = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 7));
            output_identifier(
               filename,
               line,
               column_start,
               column_end,
               scope,
               type,
               sub_type,
               identifier);
         }
      } while (result == SQLITE_ROW);

      if (result == SQLITE_DONE)
      {
         result = SQLITE_OK;
      }
   }

   if (result != SQLITE_OK)
   {
      const char *errstr = sqlite3_errstr(result);
      LOG_FMT(LERR, "Index access error (%d: %s)\n", result, errstr != NULL ? errstr : "");
      retval = false;
   }

   (void) sqlite3_finalize(stmt_lookup_identifier);

   return retval;
}
