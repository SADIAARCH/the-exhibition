#include <stdio.h>
#include "db.h"
#include "http_server.h"

#define PORT 8080

int main(int argc, char **argv) {
    const char *db_path = "artexhibit.db";
    const char *public_dir = "public";

    if (db_init(db_path) != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }

    http_server_run(PORT, public_dir);

    db_close();
    return 0;
}
