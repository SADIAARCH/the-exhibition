# The Exhibition — Art Inventory System (C + Python + SQLite + Web Dashboard)

A full-stack art exhibition manager with **two interchangeable backends** — one written in **C** (raw sockets + SQLite) and one written in **Python** (zero-dependency standard library `http.server` + `sqlite3`). Both backends implement the exact same REST API and share the same SQLite database (`artexhibit.db`) and web dashboard (`public/index.html`) with a gallery view and live charts (Chart.js).

## Project structure

```
art_exhibit/
├── src/                  # C backend (raw sockets + SQLite)
│   ├── artwork.h         # the core Artwork struct
│   ├── db.h / db.c       # SQLite CRUD layer
│   ├── json.h / json.c   # hand-rolled JSON encode/decode (no deps)
│   ├── http_server.h/.c  # minimal HTTP server (POSIX + Winsock2) + routing
│   └── main.c            # entry point
├── backend_py/
│   └── app.py            # zero-dependency Python backend (same API + SQLite DB)
├── public/
│   └── index.html        # shared gallery + charts dashboard
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

### Linux / macOS

```bash
# Debian/Ubuntu, if not already installed:
sudo apt-get install libsqlite3-dev

gcc -Wall -o art_exhibit src/main.c src/db.c src/json.c src/http_server.c \
    -lsqlite3 -D_GNU_SOURCE
```

### Windows (MSYS2 UCRT64)

```powershell
# Install SQLite3 headers and library via MSYS2, if not already installed:
C:\msys64\usr\bin\pacman.exe -S --needed mingw-w64-ucrt-x86_64-sqlite3

gcc -Wall -o art_exhibit.exe src/main.c src/db.c src/json.c src/http_server.c `
    -lsqlite3 -lws2_32
```

## Run

You can run **either** backend — both serve `public/index.html` and read/write the same `artexhibit.db` file:

### C Backend

```bash
./art_exhibit       # Linux / macOS
.\art_exhibit.exe   # Windows
```

### Python Backend (zero dependencies, Python 3.10+)

```bash
python backend_py/app.py
# Or run side-by-side with the C backend on a different port:
python backend_py/app.py --port 8081
```

Then open **http://localhost:8080** (or `:8081`) in a browser. The `public/` folder must
sit at the repository root (it's served as static files), and an
`artexhibit.db` SQLite file is created automatically on first run.

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
