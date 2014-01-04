
#include <cstdio>
#include <cstdlib>
#include <cinttypes>

#include "prototypes.h"
#include "toks_types.h"
#include "sqlite3080200.h"

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
         "CREATE TABLE Entries(Filerow INTEGER, Line INTEGER, ColumnStart INTEGER, Scope TEXT, Type TEXT, SubType TEXT, Identifier TEXT, PRIMARY KEY (Filerow, Line, ColumnStart)) WITHOUT ROWID;"
         "PRAGMA journal_mode=OFF;"
         "PRAGMA synchronous=OFF;",
         NULL,
         NULL,
         &errmsg);

      if (result != SQLITE_OK)
      {
         LOG_FMT(LERR, "index_check: access error (%d: %s)\n", result, errmsg != NULL ? errmsg : "");
         retval = false;
      }

      sqlite3_free(errmsg);
   }

   return retval;
}

bool index_prepare_for_analysis(void)
{
   int result;
   bool retval = true;

   result = sqlite3_prepare_v2(cpd.index,
                               "INSERT INTO Entries VALUES(?,?,?,?,?,?,?)",
                               -1,
                               &cpd.stmt_insert_entry,
                               NULL);

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "BEGIN",
                                  -1,
                                  &cpd.stmt_begin,
                                  NULL);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "COMMIT",
                                  -1,
                                  &cpd.stmt_commit,
                                  NULL);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "INSERT INTO Files VALUES(?,?)",
                                  -1,
                                  &cpd.stmt_insert_file,
                                  NULL);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "DELETE FROM Entries WHERE Filerow=?",
                                  -1,
                                  &cpd.stmt_prune_entries,
                                  NULL);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "UPDATE Files SET Digest=? WHERE Filename=?",
                                  -1,
                                  &cpd.stmt_change_digest,
                                  NULL);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_prepare_v2(cpd.index,
                                  "SELECT rowid,Digest FROM Files WHERE Filename=?",
                                  -1,
                                  &cpd.stmt_lookup_file,
                                  NULL);
   }

   if (result != SQLITE_OK)
   {
      const char *errstr = sqlite3_errstr(result);
      LOG_FMT(LERR, "index_prepare_for_analysis: access error (%d: %s)\n", result, errstr != NULL ? errstr : "");
      retval = false;
   }

   return retval;
}

void index_end_analysis(void)
{
   (void) sqlite3_finalize(cpd.stmt_insert_entry);
   (void) sqlite3_finalize(cpd.stmt_begin);
   (void) sqlite3_finalize(cpd.stmt_commit);
   (void) sqlite3_finalize(cpd.stmt_insert_file);
   (void) sqlite3_finalize(cpd.stmt_prune_entries);
   (void) sqlite3_finalize(cpd.stmt_change_digest);
   (void) sqlite3_finalize(cpd.stmt_lookup_file);
}

static int index_insert_file(
   const char *digest,
   const char *filename,
   sqlite3_int64 *filerow)
{
   int result;

   result = sqlite3_bind_text(cpd.stmt_insert_file,
                              1,
                              digest,
                              -1,
                              SQLITE_STATIC);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(cpd.stmt_insert_file,
                                 2,
                                 filename,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(cpd.stmt_insert_file);
      if (result == SQLITE_DONE)
      {
         result = sqlite3_reset(cpd.stmt_insert_file);
      }
   }

   *filerow = sqlite3_last_insert_rowid(cpd.index);

   return result;
}

static int index_prune_entries(sqlite3_int64 filerow)
{
   int result;

   result = sqlite3_bind_int64(cpd.stmt_prune_entries,
                               1,
                               filerow);

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(cpd.stmt_prune_entries);
      if (result == SQLITE_DONE)
      {
         result = sqlite3_reset(cpd.stmt_prune_entries);
      }
   }

   return result;
}

static int index_replace_file(const char *digest, const char *filename)
{
   int result;

   result = sqlite3_bind_text(cpd.stmt_change_digest,
                              1,
                              digest,
                              -1,
                              SQLITE_STATIC);

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_text(cpd.stmt_change_digest,
                                 2,
                                 filename,
                                 -1,
                                 SQLITE_STATIC);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(cpd.stmt_change_digest);
      if (result == SQLITE_DONE)
      {
         result = sqlite3_reset(cpd.stmt_change_digest);
      }
   }

   return result;
}

/* Returns true if the file needs to be analyzed */
bool index_prepare_for_file(fp_data& fpd)
{
   int result;
   bool retval = true;
   sqlite3_int64 filerow = 0;

   result = sqlite3_bind_text(cpd.stmt_lookup_file,
                              1,
                              fpd.filename,
                              -1,
                              SQLITE_STATIC);

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(cpd.stmt_lookup_file);
   }

   if (result == SQLITE_ROW)
   {
      filerow = sqlite3_column_int64(cpd.stmt_lookup_file, 0);
      const char *ingest =
         (const char *) sqlite3_column_text(cpd.stmt_lookup_file, 1);

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
      result = index_insert_file(fpd.digest, fpd.filename, &filerow);
      LOG_FMT(LNOTE, "File %s(%s) does not exist in index, inserted at filerow %"PRId64"\n", fpd.filename, fpd.digest, (int64_t) filerow);
   }

   if (result == SQLITE_OK)
   {
      result = sqlite3_bind_int64(cpd.stmt_insert_entry,
                                  1,
                                  filerow);
   }

   if (result != SQLITE_OK)
   {
      const char *errstr = sqlite3_errstr(result);
      LOG_FMT(LERR, "index_prepare_for_file: access error (%d: %s)\n", result, errstr != NULL ? errstr : "");
      retval = false;
   }

   (void) sqlite3_reset(cpd.stmt_lookup_file);

   return retval;
}

void index_begin_file(fp_data& fpd)
{
   (void) sqlite3_reset(cpd.stmt_begin);
   (void) sqlite3_step(cpd.stmt_begin);
}

void index_end_file(fp_data& fpd)
{
   (void) sqlite3_reset(cpd.stmt_commit);
   (void) sqlite3_step(cpd.stmt_commit);
}

bool index_insert_entry(
   fp_data& fpd,
   UINT32 line,
   UINT32 column_start,
   const char *scope,
   const char *type,
   const char *sub_type,
   const char *identifier)
{
   bool retval = true;
   int result;

   result = sqlite3_bind_int64(cpd.stmt_insert_entry,
                               2,
                               line);
   result |= sqlite3_bind_int64(cpd.stmt_insert_entry,
                                3,
                                column_start);
   result |= sqlite3_bind_text(cpd.stmt_insert_entry,
                               4,
                               scope,
                               -1,
                               SQLITE_STATIC);
   result |= sqlite3_bind_text(cpd.stmt_insert_entry,
                               5,
                               type,
                               -1,
                               SQLITE_STATIC);
   result |= sqlite3_bind_text(cpd.stmt_insert_entry,
                               6,
                               sub_type,
                               -1,
                               SQLITE_STATIC);
   result |= sqlite3_bind_text(cpd.stmt_insert_entry,
                               7,
                               identifier,
                               -1,
                               SQLITE_STATIC);

   if (result == SQLITE_OK)
   {
      result = sqlite3_step(cpd.stmt_insert_entry);
      if (result == SQLITE_DONE)
      {
         result = sqlite3_reset(cpd.stmt_insert_entry);
      }
   }

   if (result != SQLITE_OK)
   {
      const char *errstr = sqlite3_errstr(result);
      LOG_FMT(LERR, "index_insert_entry: access error (%d: %s)\n", result, errstr != NULL ? errstr : "");
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
                               "SELECT Files.Filename,Entries.Line,Entries.ColumnStart,Entries.Scope,Entries.Type,Entries.SubType,Entries.Identifier "
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
            const char *scope = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 3));
            const char *type = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 4));
            const char *sub_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 5));
            const char *identifier = reinterpret_cast<const char*>(sqlite3_column_text(stmt_lookup_identifier, 6));
            output_identifier(
               filename,
               line,
               column_start,
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
      LOG_FMT(LERR, "index_lookup_identifier: access error (%d: %s)\n", result, errstr != NULL ? errstr : "");
      retval = false;
   }

   (void) sqlite3_finalize(stmt_lookup_identifier);

   return retval;
}
