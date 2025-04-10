// cppcheck-suppress-file unusedStructMembers

#ifndef DATABASE_H
#define DATABASE_H

#include <ndbm.h>
#include <stdint.h>

#ifdef __APPLE__
typedef size_t datum_size;
#else
typedef int datum_size;
#endif

typedef struct
{
    const void *dptr;
    datum_size  dsize;
} const_datum;

#define MAKE_CONST_DATUM(str) ((const_datum){(str), (datum_size)strlen(str) + 1})
#define DB_RECORDS "db_records"

int      db_insert(DBM *db, const char *key, const uint8_t *buf, size_t size, int *err);
uint8_t *db_fetch(DBM *db, const char *key, size_t *len, int *err);

int  db_init(DBM **db, const char *filepath, int *err);
void db_destroy(DBM **db);

#endif
