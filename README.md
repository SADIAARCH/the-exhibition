# The Exhibition — Art Inventory System (C + SQLite + Web Dashboard)

A full-stack art exhibition manager: the backend is written entirely in C,
storing artworks in a SQLite database and serving a JSON API over raw
sockets. The frontend is a single HTML dashboard with a gallery view and
live charts (via Chart.js), talking to the C server through `fetch`.

## Project structure

```
art_exhibit/
├── src/
│   ├── artwork.h        # the core Artwork struct
│   ├── db.h / db.c       # SQLite CRUD layer
│   ├── json.h / json.c   # hand-rolled JSON encode/decode (no deps)
│   ├── http_server.h/.c  # minimal HTTP server (sockets) + routing
│   └── main.c            # entry point
├── public/
│   └── index.html         # gallery + charts dashboard (served by the C app)
└── README.md
```

## The struct

```c
typedef struct {
    int    id;
    char   title[100];
    char   artist[100];
    char   medium[50];   // "Watercolor", "Acrylic", "Sketch", "Pen Sketch", "Oil", ...
    double price;
} Artwork;
```

## Build

Requires `gcc` and the SQLite3 dev headers.

```bash
# Debian/Ubuntu, if not already installed:
sudo apt-get install libsqlite3-dev

gcc -Wall -o art_exhibit src/main.c src/db.c src/json.c src/http_server.c \
    -lsqlite3 -D_GNU_SOURCE
```

## Run

```bash
./art_exhibit
```

Then open **http://localhost:8080** in a browser. The `public/` folder must
sit next to the executable (it's served as static files), and a
`artexhibit.db` SQLite file is created automatically on first run in the
working directory.

## API

| Method | Path                     | Description                          |
|--------|--------------------------|---------------------------------------|
| GET    | `/api/artworks`          | List all artworks (JSON array)        |
| GET    | `/api/artworks/stats`    | Aggregated stats per medium, for charts |
| POST   | `/api/artworks`          | Add an artwork (JSON body)            |
| PUT    | `/api/artworks/:id`      | Edit an artwork (JSON body)           |
| DELETE | `/api/artworks/:id`      | Delete an artwork                     |

Example add:

```bash
curl -X POST http://localhost:8080/api/artworks \
  -H "Content-Type: application/json" \
  -d '{"title":"Evening on the Buriganga","artist":"Sadia","medium":"Watercolor","price":45.50}'
```

## Notes on design choices

- **SQLite** was chosen over a custom file format so the data survives
  restarts and supports real queries, while staying embeddable (no separate
  DB server to run).
- **The HTTP server is hand-written with POSIX sockets** rather than a
  library, so the whole stack — routing, JSON, and persistence — is your own
  C code, single-threaded and easy to trace through for a course
  submission or viva.
- **JSON parsing is intentionally minimal** — it only needs to understand
  the flat shape the dashboard sends (`title`, `artist`, `medium`, `price`),
  not arbitrary JSON. If you extend the schema, extend `json_parse_artwork`
  in `json.c` accordingly.
- **Extending the medium list**: add options in `public/index.html`'s
  `<select id="f-medium">` — the backend stores whatever string you send,
  so no backend changes are needed for new medium types.
