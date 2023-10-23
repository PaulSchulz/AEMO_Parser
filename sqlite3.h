#include <sqlite3.h>

#define SQLITEDB "AEMO.db"

sqlite3* aemo_sqlite3_open(char* database);
int aemo_sqlite3_write(sqlite3* db, char* data);
int aemo_sqlite3_close(sqlite3* db);
