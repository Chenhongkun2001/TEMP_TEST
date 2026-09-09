#include <stdio.h>
#include "sqlite3.h"

int main(void)
{
    sqlite3 *db = NULL;
    char *err = NULL;

    int td = sqlite3_open("skf.db", &db);

    if (td != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const char *sql = "DROP TABLE IF EXISTS Cars;"
                      "CREATE TABLE Cars(Id INT, Name TEXT, Price INT);"
                      "INSERT INTO Cars VALUES(1, 'Audi', 52642);"
                      "INSERT INTO Cars VALUES(2, 'Mercedes', 57127);"
                      "INSERT INTO Cars VALUES(3, 'Skoda', 9000);"
                      "INSERT INTO Cars VALUES(4, 'Volvo', 29000);";

    td = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (td != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 1;
    }
    sqlite3_close(db);
    return 0;
}


