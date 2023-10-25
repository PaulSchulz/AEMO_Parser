#include <sqlite3.h>

// #include "parser.h"
#define SQLITEDB "aemo.db"

sqlite3* aemo_sqlite3_open(char* database);
int aemo_sqlite3_initialise(sqlite3* db);
int aemo_sqlite3_write(sqlite3* db, struct AEMO* data, int number_tries);
int aemo_sqlite3_close(sqlite3* db);
