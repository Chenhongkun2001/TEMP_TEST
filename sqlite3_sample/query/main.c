#include <stdio.h>
#include "sqlite3.h"

int callback(void *, int, char **, char **);

int main(void)
{
    sqlite3 *db = NULL;
    char *err_msg = NULL;

    int rc = sqlite3_open("skf.db", &db);
    if (rc != SQLITE_OK)
    {

        fprintf(stderr, "Cannot open database: %s\n",
                sqlite3_errmsg(db));
        sqlite3_close(db);

        return 1;
    }

    const char *sql = "SELECT * FROM Cars";

    rc = sqlite3_exec(db, sql, callback, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {

        fprintf(stderr, "Failed to select data\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    sqlite3_close(db);

    return 0;
}

int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    NotUsed = NULL;

    for (int i = 0; i < argc; ++i)
    {
        printf("%s = %s\n", azColName[i], (argv[i] ? argv[i] : "NULL"));
    }

    printf("\n");

    return 0;
}
