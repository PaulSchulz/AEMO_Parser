#include "stdio.h"
#include "stdlib.h"
#include "string.h"
// #include "MQTTClient.h"
// #include "mqtt.h"
#include "sqlite3.h"

// #define QOS         1
// #define TIMEOUT     10000L

sqlite3* aemo_sqlite3_open(char* database)
{
    sqlite3 *db;
    int rc;

    // Open or create the database
    rc = sqlite3_open(database, &db);

    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    return(db);
}

int aemo_sqlite3_write(sqlite3* db, char* data)
{
    int rc;

    // Your data record and INSERT statement here
    // const char *data = "YourData";
    const char *sql = "INSERT INTO your_table (data) VALUES (?);";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return(1);
    }

    sqlite3_bind_text(stmt, 1, data, -1, SQLITE_STATIC);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return(1);
    }

    // Finalize the statement and close the database
    sqlite3_finalize(stmt);

    return 0;
}

int aemo_sqlite3_close(sqlite3* db)
{
    sqlite3_close(db);
    return 0;
}
