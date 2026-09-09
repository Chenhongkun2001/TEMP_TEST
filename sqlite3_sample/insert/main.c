#include <stdio.h>
#include "sqlite3.h"

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
    const char *sql = "INSERT INTO Cars VALUES(5, 'BYD', 2500);"
                      "INSERT INTO Cars VALUES(6, 'NIO', 3000);";

    if ((rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg)) != SQLITE_OK)
    {
        printf("Sql error is =%s \n", err_msg);
        return 0;
    }
    else
    {
        printf("Insert table successfully\n");
    }

    sqlite3_close(db);

    return 0;
}
