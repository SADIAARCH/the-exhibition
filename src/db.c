#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "db.h"

static sqlite3 *g_db = NULL;

int db_init(const char *path) {
    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS artworks ("
        "  id     INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title  TEXT NOT NULL,"
        "  artist TEXT NOT NULL,"
        "  medium TEXT NOT NULL,"
        "  price  REAL NOT NULL DEFAULT 0"
        ");";

    char *errmsg = NULL;
    rc = sqlite3_exec(g_db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

void db_close(void) {
    if (g_db) sqlite3_close(g_db);
}

int db_add_artwork(Artwork *a) {
    const char *sql =
        "INSERT INTO artworks (title, artist, medium, price) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, a->title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, a->artist, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, a->medium, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, a->price);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;

    return (int)sqlite3_last_insert_rowid(g_db);
}

int db_update_artwork(int id, Artwork *a) {
    const char *sql =
        "UPDATE artworks SET title=?, artist=?, medium=?, price=? WHERE id=?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, a->title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, a->artist, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, a->medium, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, a->price);
    sqlite3_bind_int(stmt, 5, id);

    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE || changes == 0) return -1;
    return 0;
}

int db_delete_artwork(int id) {
    const char *sql = "DELETE FROM artworks WHERE id=?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE || changes == 0) return -1;
    return 0;
}

Artwork *db_get_all(int *count) {
    const char *sql = "SELECT id, title, artist, medium, price FROM artworks ORDER BY id;";
    sqlite3_stmt *stmt;
    *count = 0;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;

    int capacity = 16;
    Artwork *list = malloc(sizeof(Artwork) * capacity);
    int n = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (n >= capacity) {
            capacity *= 2;
            list = realloc(list, sizeof(Artwork) * capacity);
        }
        Artwork *a = &list[n];
        a->id = sqlite3_column_int(stmt, 0);
        snprintf(a->title, sizeof(a->title), "%s", (const char *)sqlite3_column_text(stmt, 1));
        snprintf(a->artist, sizeof(a->artist), "%s", (const char *)sqlite3_column_text(stmt, 2));
        snprintf(a->medium, sizeof(a->medium), "%s", (const char *)sqlite3_column_text(stmt, 3));
        a->price = sqlite3_column_double(stmt, 4);
        n++;
    }
    sqlite3_finalize(stmt);

    *count = n;
    return list;
}
