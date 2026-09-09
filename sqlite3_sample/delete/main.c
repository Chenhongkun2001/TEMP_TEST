#include <stdio.h>
#include "sqlite3.h"

int main(int argc, char const *argv[])
{
	sqlite3 *db;
	int ret = 0;
	char *sql;
	char *err_msg = NULL;
	if ((ret = sqlite3_open("skf.db",&db)) != SQLITE_OK) 
	{
		printf("open error!\n");
		return -1;
	}
	else
	{
		printf("open database successfully\n");
	}


sql = "DELETE from Cars where ID=2; " \
         "SELECT * from Cars";

     if ((ret = sqlite3_exec(db,sql,NULL,NULL,&err_msg)) != SQLITE_OK )
     {
     	printf("Sql error is =%s \n",err_msg);
     	return 0;
     }

     else
     {
     	printf("Delete successfully\n");
     }

	sqlite3_close(db);
	return 0;
}

