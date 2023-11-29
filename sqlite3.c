#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <time.h>
#include "parser.h"
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

int aemo_sqlite3_initialise(sqlite3* db)
{
    int rc;

    // Query to check if the table exists in sqlite_master
    const char *checkTableSQL =
        "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, checkTableSQL, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;

    }

    const char *tableName = "aemo"; // Replace with the name of the table you want to check

    sqlite3_bind_text(stmt, 1, tableName, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        printf("# Table exists: %s\n", tableName);

    } else {
        printf("Table does not exist.\n");
        printf("Creating table: %s\n", tableName);

        // Your data record and INSERT statement here
        // const char *data = "YourData";
        const char *sqlstatement =
            "CREATE TABLE aemo ("
            "timestamp DATETIME,"
            "tries INTEGER,"
            "region TEXT,"
            "settlement DATETIME,"
            "price REAL,"
            "total_demand REAL,"
            "net_interchange REAL,"
            "scheduled_generation REAL,"
            "semi_scheduled_generation REAL);";

        rc = sqlite3_exec(db, sqlstatement, 0, 0, 0);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
        return 1;
        }
    }

    return 0;
}

int aemo_sqlite3_write(sqlite3* db, struct AEMO * aemo, int number_tries)
{
    int rc;

    // Your data record and INSERT statement here
    // const char *data = "YourData";
    const char *sql =
        "INSERT INTO aemo ("
        "  timestamp,"
        "  tries,"
        "  region,"
        "  settlement,"
        "  price,"
        "  total_demand,"
        "  net_interchange,"
        "  scheduled_generation,"
        "  semi_scheduled_generation"
        ") VALUES (?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return(1);
    }

    const char sql_val1[64];
    const char sql_val2[64];
    const char sql_val3[64];
    const char sql_val4[64];
    const char sql_val5[64];
    const char sql_val6[64];
    const char sql_val7[64];
    const char sql_val8[64];
    const char sql_val9[64];

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        perror("clock_gettime");
        return 1;
    }

    time_t now;
    struct tm timeinfo;

    //time(&now);
    now = ts.tv_sec;
    localtime_r(&now, &timeinfo);

    sprintf((char * restrict)sql_val1,
            "%04d-%02d-%02d %02d:%02d:%02d.%09ld",
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec,
            ts.tv_nsec);
    sqlite3_bind_text(stmt, 1, sql_val1, -1, SQLITE_STATIC);

    sprintf((char * restrict)sql_val2, "%d", number_tries);
    sqlite3_bind_text(stmt, 2, sql_val2, -1, SQLITE_STATIC);

    sprintf((char * restrict)sql_val3, "%s", aemo->region);
    sqlite3_bind_text(stmt, 3, sql_val3, -1, SQLITE_STATIC);

    sprintf((char * restrict)sql_val4,
            "%04d-%02d-%02d %02d:%02d:%02d",
            aemo->settlement.tm_year + 1900,
            aemo->settlement.tm_mon + 1,
            aemo->settlement.tm_mday,
            aemo->settlement.tm_hour,
            aemo->settlement.tm_min,
            aemo->settlement.tm_sec);
    sqlite3_bind_text(stmt, 4, sql_val4, -1, SQLITE_STATIC);

    sprintf((char * restrict)sql_val5, "%.02f", aemo->price);
    sqlite3_bind_text(stmt, 5, sql_val5, -1, SQLITE_STATIC);

    sprintf((char * restrict)sql_val6, "%.02f", aemo->totaldemand);
    sqlite3_bind_text(stmt, 6, sql_val6, -1, SQLITE_STATIC);

    sprintf((char * restrict)sql_val7, "%.02f", aemo->netinterchange);
    sqlite3_bind_text(stmt, 7, sql_val7, -1, SQLITE_STATIC);

    sprintf((char * restrict)sql_val8, "%.02f", aemo->scheduledgeneration);
    sqlite3_bind_text(stmt, 8, sql_val8, -1, SQLITE_STATIC);

    sprintf((char * restrict)sql_val9, "%.02f", aemo->semischeduledgeneration);
    sqlite3_bind_text(stmt, 9, sql_val9, -1, SQLITE_STATIC);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return(1);
    }

    // Finalize the statement and close the database
    // sqlite3_finalize(stmt);
    return 0;
}

int aemo_all_sqlite3_write(sqlite3* db, struct AEMO_ALL* aemo_all) {
    for(int i=0; i<MAX_REGIONS; i++) {
        aemo_sqlite3_write(db, &(aemo_all->region[i]), aemo_all->number_tries);
    }

    return 0;
}


int aemo_sqlite3_close(sqlite3* db)
{
    sqlite3_close(db);
    return 0;
}
