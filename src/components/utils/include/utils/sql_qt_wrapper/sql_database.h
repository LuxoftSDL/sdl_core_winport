#ifndef SRC_COMPONENTS_POLICY_SQLITE_WRAPPER_INCLUDE_SQL_QT_WRAPPER_SQL_DATABASE
#define SRC_COMPONENTS_POLICY_SQLITE_WRAPPER_INCLUDE_SQL_QT_WRAPPER_SQL_DATABASE

#include <string>

#include <QtSql/QSqlDatabase>

#include "utils/lock.h"
#include "utils/sql_qt_wrapper/sql_error.h"

namespace utils {
namespace dbms {

/**
 * Represents a connection to a database.
 */
class SQLDatabase {
 public:
  SQLDatabase();
  explicit SQLDatabase(const std::string& filename);
  ~SQLDatabase();

  /**
   * Opens connection to the temporary in-memory database
   * @return true if successfully
   */
  bool Open();

  /**
   * Closes connection to the database
   */
  void Close();

  /**
   * Begins a transaction on the database
   * @return true if successfully
   */
  bool BeginTransaction();

  /**
   * Commits a transaction to the database
   * @return true if successfully
   */
  bool CommitTransaction();

  /**
   * Rolls back a transaction on the database
   * @return true if successfully
   */
  bool RollbackTransaction();

  /**
   * Gets information about the last error that occurred on the database
   * @return last error
   */
  SQLError LastError() const;

  /**
   * @brief NoErrors Indicate the status of the last executed operation.
   *
   * @return true in case last operation has been executed successfully, false otherwise.
   */
  bool NoErrors() const;

  /**
   * Sets path to database
   * If the database is already opened then need reopen it
   */
  void set_path(const std::string& path);

  /**
   * @brief get_path databse location path.
   *
   * @return the path to the database location
   */
  std::string get_path() const;

  /**
   * Checks if database is read/write
   * @return true if database is read/write
   */
  bool IsReadWrite();

  /**
   * Call backup for opened DB
   */
  bool Backup();

  operator QSqlDatabase() const;

 protected:
 private:

  QSqlDatabase db_;
  /**
   * Lock for guarding connection to database
   */
  sync_primitives::Lock conn_lock_;

  /**
   * The filename of database
   */
  std::string databasename_;

  /**
   * The last error that occurred on the database
   */
  int error_;

  /**
   *  The temporary in-memory database
   *  @see SQLite manual
   */
  static const std::string kInMemory;

  /**
   * The extension of filename of database
   */
  static const std::string kExtension;

  /**
   * Execs query for internal using in this class
   * @param query sql query without return results
   * @return true if query was executed successfully
   */
  inline bool Exec(const std::string& query);

};

}  // namespace dbms
}  // namespace utils

#endif  // SRC_COMPONENTS_POLICY_SQLITE_WRAPPER_INCLUDE_SQL_QT_WRAPPER_SQL_DATABASE
