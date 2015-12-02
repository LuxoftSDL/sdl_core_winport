/*
 * Copyright (c) 2013, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "sqlite_wrapper/sql_query.h"
#include <sqlite3.h>
#include <cassert>
#include "sqlite_wrapper/sql_database.h"



namespace utils {
namespace dbms {

SQLQuery::SQLQuery(SQLDatabase* db)
    : db_(*db),
      query_(""),
#ifdef WIN_NATIVE
      statement_(NULL),
#endif // WIN_NATIVE
      error_(SQLITE_OK) {
}

SQLQuery::~SQLQuery() {
  Finalize();
}

bool SQLQuery::Prepare(const std::string& query) {
#if defined WIN_NATIVE
  Finalize();
  sync_primitives::AutoLock auto_lock(statement_lock_);
  if (statement_) return false;
  error_ = sqlite3_prepare(db_.conn(), query.c_str(),
                           static_cast<int>(query.length()),
                           &statement_, NULL);
  query_ = query;
  return error_ == SQLITE_OK;
#endif // WIN_NATIVE
return true;
}

bool SQLQuery::Exec() {
#if defined WIN_NATIVE
  error_ = sqlite3_step(statement_);
  return error_ == SQLITE_ROW || error_ == SQLITE_DONE;
#endif // WIN_NATIVE
return true;
}

bool SQLQuery::Next() {
#if defined WIN_NATIVE
  error_ = sqlite3_step(statement_);
  return error_ == SQLITE_ROW;
#endif // WIN_NATIVE
return true;
}

bool SQLQuery::Reset() {
#if defined WIN_NATIVE
  error_ = sqlite3_reset(statement_);
  return error_ == SQLITE_OK;
#endif // WIN_NATIVE
return true;
}

void SQLQuery::Finalize() {
#if defined WIN_NATIVE
  sync_primitives::AutoLock auto_lock(statement_lock_);
  error_ = sqlite3_finalize(statement_);
  if (error_ == SQLITE_OK) {
    statement_ = NULL;
  }
#endif // WIN_NATIVE
}

bool SQLQuery::Exec(const std::string& query) {
#if defined WIN_NATIVE
  query_ = query;
  error_ = sqlite3_exec(db_.conn(), query.c_str(), NULL, NULL, NULL);
  return error_ == SQLITE_OK;
#endif // WIN_NATIVE
return true;
}

void SQLQuery::Bind(int pos, int value) {
#if defined WIN_NATIVE
  // In SQLite the number of position for binding starts since 1.
  error_ = sqlite3_bind_int(statement_, pos + 1, value);
#endif // WIN_NATIVE
}

void SQLQuery::Bind(int pos, int64_t value) {
#if defined WIN_NATIVE
  // In SQLite the number of position for binding starts since 1.
  error_ = sqlite3_bind_int64(statement_, pos + 1, value);
#endif // WIN_NATIVE
}

void SQLQuery::Bind(int pos, double value) {
#if defined WIN_NATIVE
  // In SQLite the number of position for binding starts since 1.
  error_ = sqlite3_bind_double(statement_, pos + 1, value);
#endif // WIN_NATIVE
}

void SQLQuery::Bind(int pos, bool value) {
  Bind(pos, static_cast<int>(value));
}

void SQLQuery::Bind(int pos, const std::string& value) {
#if defined WIN_NATIVE
  // In SQLite the number of position for binding starts since 1.
  error_ = sqlite3_bind_text(statement_, pos + 1, value.c_str(),
                             static_cast<int>(value.length()),
                             SQLITE_TRANSIENT);
#endif // WIN_NATIVE
}

bool SQLQuery::GetBoolean(int pos) const {
  return GetInteger(pos) != 0;
}

int SQLQuery::GetInteger(int pos) const {
#if defined WIN_NATIVE
  return sqlite3_column_int(statement_, pos);
#endif // WIN_NATIVE
return 0;
}

uint32_t SQLQuery::GetUInteger(int pos) const {
#if defined WIN_NATIVE
  return static_cast<uint32_t>(
      sqlite3_column_int64(statement_, pos));
#endif // WIN_NATIVE
return 0;
}

int64_t SQLQuery::GetLongInt(int pos) const {
#if defined WIN_NATIVE
  return static_cast<int64_t>(
      sqlite3_column_int64(statement_, pos));
#endif // WIN_NATIVE
return 0;
}

double SQLQuery::GetDouble(int pos) const {
#if defined WIN_NATIVE
  return sqlite3_column_double(statement_, pos);
#endif // WIN_NATIVE
return 0;
}

std::string SQLQuery::GetString(int pos) const {
#if defined WIN_NATIVE
  const unsigned char* str = sqlite3_column_text(statement_, pos);
  return str ? reinterpret_cast<const char*>(str) : "";
#endif // WIN_NATIVE
return "";
}

const std::string& SQLQuery::query() const {
  // TODO(KKolodiy): may return string query with value of arguments
  return query_;
}

bool SQLQuery::IsNull(int pos) const {
#if defined WIN_NATIVE
  return sqlite3_column_type(statement_, pos) == SQLITE_NULL;
#endif // WIN_NATIVE
return true;
}

void SQLQuery::Bind(int pos) {
#if defined WIN_NATIVE
  // In SQLite the number of position for binding starts since 1.
  error_ = sqlite3_bind_null(statement_, pos + 1);
#endif // WIN_NATIVE
}

SQLError SQLQuery::LastError() const {
  return SQLError(Error(error_));
}

int64_t SQLQuery::LastInsertId() const {
#if defined WIN_NATIVE
  return sqlite3_last_insert_rowid(db_.conn());
#endif // WIN_NATIVE
return 0;
}

}  // namespace dbms
}  // namespace utils

