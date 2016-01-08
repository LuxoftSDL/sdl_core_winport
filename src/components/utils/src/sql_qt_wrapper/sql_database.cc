#include "sql_qt_wrapper/sql_database.h"

#include <QSqlError>
#include <QSqlQuery>

namespace utils {
namespace dbms {

SQLDatabase::SQLDatabase(const std::string& filename)
    : databasename_(filename) {
  db_ = QSqlDatabase::addDatabase("QSQLITE");
}

SQLDatabase::~SQLDatabase() {
  Close();
}

bool SQLDatabase::Open() {
  db_.setDatabaseName(databasename_.c_str());
  return db_.open();
}

void SQLDatabase::Close() {
  db_.close();
}

bool SQLDatabase::BeginTransaction() {
  return db_.transaction();
}

bool SQLDatabase::CommitTransaction() {
  return db_.commit();
}

bool SQLDatabase::RollbackTransaction() {
  return db_.rollback();
}

SQLError SQLDatabase::LastError() const {
  return SQLError(db_.lastError());
}

bool SQLDatabase::HasErrors() const {
  return db_.lastError().type() != QSqlError::NoError;
}

void SQLDatabase::set_path(const std::string& path) {
  databasename_ = path + databasename_;
}

std::string SQLDatabase::get_path() const {
  return databasename_;
}

bool SQLDatabase::IsReadWrite() {
  return true;
}

bool SQLDatabase::Backup() {
  return true;
}

SQLDatabase::operator QSqlDatabase() const {
  return db_;
}

bool SQLDatabase::Exec(const std::string& query) {
  return true;
}

}  // namespace dbms
}  // namespace utils
