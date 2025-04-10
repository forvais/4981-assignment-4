#include "ndbm/database.h"
#include "logger.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int db_insert(DBM *db, const char *key, const uint8_t *buf, size_t size, int *err)
{
    const_datum key_datum = MAKE_CONST_DATUM(key);
    datum       value_datum;
    int         result;

    if(db == NULL || key == NULL || buf == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    errno            = 0;
    value_datum.dptr = (char *)calloc(size, sizeof(char));
    if(value_datum.dptr == NULL)
    {
        seterr(errno);
        return -2;
    }

    memcpy(value_datum.dptr, buf, size);
    value_datum.dsize = (int)size;

    result = dbm_store(db, *(datum *)&key_datum, value_datum, DBM_REPLACE);

    if(result == -1)
    {
        printf("%d\n", dbm_error(db));
    }

    free(value_datum.dptr);
    return result;
}

uint8_t *db_fetch(DBM *db, const char *key, size_t *len, int *err)
{
    datum       fetched;
    const_datum key_datum = MAKE_CONST_DATUM(key);

    if(db == NULL || key == NULL)
    {
        seterr(EINVAL);
        return NULL;
    }

    // Read data from db
    fetched = dbm_fetch(db, *(datum *)&key_datum);
    if(fetched.dptr == NULL)
    {
        seterr(EINVAL);
        return NULL;
    }

    if(fetched.dsize < 0)
    {
        log_error("db_fetch_bytes::%s: Returned size is less than zero.\n", key);
        seterr(ERANGE);
        return NULL;
    }

    if((unsigned long)fetched.dsize > SIZE_MAX)
    {
        log_error("db_fetch_bytes::%s: Returned size is greater than int max.\n", key);
        seterr(ERANGE);
        return NULL;
    }

    if(len)
    {
        *len = (size_t)fetched.dsize;
    }

    return (uint8_t *)fetched.dptr;
}

int db_init(DBM **db, const char *filepath, int *err)
{
    char *database_name = strdup(filepath);

    // Open NDBM database
    log_debug("Opening DBM database at %s\n", filepath);

    errno = 0;
    *db   = dbm_open(database_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(*db == NULL)
    {
        seterr(errno);
        free(database_name);
        return -1;
    }

    free(database_name);
    return 0;
}

void db_destroy(DBM **db)
{
    dbm_close(*db);
    *db = NULL;
}
