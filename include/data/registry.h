#ifndef BUH_DATA_REGISTRY_H
#define BUH_DATA_REGISTRY_H

#include "core/types.h"

int db_load(Database *db);
int find_weapon(Database *db, const char *id);

#endif
