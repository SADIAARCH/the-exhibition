#ifndef DB_H
#define DB_H

#include "artwork.h"

int  db_init(const char *path);
void db_close(void);

/* CRUD */
int  db_add_artwork(Artwork *a);              /* returns new id, -1 on error */
int  db_update_artwork(int id, Artwork *a);   /* returns 0 on success */
int  db_delete_artwork(int id);               /* returns 0 on success */

/* Returns a malloc'd array of Artwork; caller frees. *count set to length. */
Artwork *db_get_all(int *count);

#endif
