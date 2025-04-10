#include "logger.h"
#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char *argv[])
{
    DBM  *db;
    datum key;
    char *name = strdup(argv[1]);

    (void)(argc);

    db = dbm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(!db)
    {
        perror("main::dbm_open");
        free(name);
        return 1;
    }

    for(key = dbm_firstkey(db); key.dptr != NULL; key = dbm_nextkey(db))
    {
        datum val = dbm_fetch(db, key);
        log_info("%.*s: %.*s\n", key.dsize, key.dptr, val.dsize, val.dptr);
    }

    dbm_close(db);
    free(name);
    return 0;
}
