"""Zero-dependency Python backend for The Exhibition.

Implements the exact same REST API and SQLite schema as the C backend (`src/`),
serving `public/index.html` and storing artworks in `artexhibit.db`.

Run from the repository root:
    python backend_py/app.py
"""

import argparse
import json
import mimetypes
import sqlite3
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_DB = ROOT_DIR / "artexhibit.db"
DEFAULT_PUBLIC = ROOT_DIR / "public"


def init_db(db_path: Path) -> None:
    with sqlite3.connect(db_path) as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS artworks (
                id     INTEGER PRIMARY KEY AUTOINCREMENT,
                title  TEXT NOT NULL,
                artist TEXT NOT NULL,
                medium TEXT NOT NULL,
                price  REAL NOT NULL DEFAULT 0
            );
            """
        )


def parse_artwork_id(path: str) -> int | None:
    prefix = "/api/artworks/"
    if not path.startswith(prefix):
        return None
    raw = path[len(prefix) :]
    if not raw.isdigit():
        return None
    return int(raw)


def make_handler(db_path: Path, public_dir: Path):
    public_root = public_dir.resolve()

    class ExhibitionHandler(BaseHTTPRequestHandler):
        def _send_bytes(self, status: int, content_type: str, data: bytes) -> None:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.send_header("Connection", "close")
            self.end_headers()
            if data:
                self.wfile.write(data)

        def _send_json(self, status: int, payload) -> None:
            body = json.dumps(payload).encode("utf-8")
            self._send_bytes(status, "application/json", body)

        def _read_json(self) -> dict | None:
            try:
                length = int(self.headers.get("Content-Length", "0"))
            except ValueError:
                return None
            raw = self.rfile.read(max(0, length))
            try:
                data = json.loads(raw.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                return None
            return data if isinstance(data, dict) else None

        def _connect(self) -> sqlite3.Connection:
            conn = sqlite3.connect(db_path)
            conn.row_factory = sqlite3.Row
            return conn

        def _serve_static(self, req_path: str) -> None:
            rel = "index.html" if req_path == "/" else req_path.lstrip("/")
            candidate = (public_root / rel).resolve()
            if public_root not in candidate.parents and candidate != public_root:
                self._send_json(403, {"error": "forbidden"})
                return
            if not candidate.is_file():
                self._send_json(404, {"error": "not found"})
                return
            content_type, _ = mimetypes.guess_type(str(candidate))
            self._send_bytes(200, content_type or "application/octet-stream", candidate.read_bytes())

        def do_OPTIONS(self) -> None:
            self._send_bytes(204, "text/plain", b"")

        def do_GET(self) -> None:
            path = urlparse(self.path).path

            if path == "/api/artworks":
                with self._connect() as conn:
                    rows = conn.execute(
                        "SELECT id, title, artist, medium, price FROM artworks ORDER BY id"
                    ).fetchall()
                artworks = [
                    {
                        "id": r["id"],
                        "title": r["title"],
                        "artist": r["artist"],
                        "medium": r["medium"],
                        "price": round(float(r["price"]), 2),
                    }
                    for r in rows
                ]
                self._send_json(200, artworks)
                return

            if path == "/api/artworks/stats":
                with self._connect() as conn:
                    rows = conn.execute(
                        "SELECT medium, price FROM artworks ORDER BY id"
                    ).fetchall()
                by_medium: dict[str, dict] = {}
                total_value = 0.0
                for r in rows:
                    price = float(r["price"])
                    total_value += price
                    m = r["medium"]
                    if m not in by_medium:
                        by_medium[m] = {"medium": m, "count": 0, "total_price": 0.0}
                    by_medium[m]["count"] += 1
                    by_medium[m]["total_price"] += price

                medium_list = [
                    {
                        "medium": v["medium"],
                        "count": v["count"],
                        "total_price": round(v["total_price"], 2),
                    }
                    for v in by_medium.values()
                ]
                self._send_json(
                    200,
                    {
                        "total_artworks": len(rows),
                        "total_value": round(total_value, 2),
                        "by_medium": medium_list,
                    },
                )
                return

            self._serve_static(path)

        def do_POST(self) -> None:
            path = urlparse(self.path).path
            if path != "/api/artworks":
                self._send_json(404, {"error": "not found"})
                return

            data = self._read_json()
            if data is None:
                self._send_json(400, {"error": "invalid json"})
                return

            title = str(data.get("title", "")).strip()
            artist = str(data.get("artist", "")).strip()
            medium = str(data.get("medium", "")).strip()
            try:
                price = float(data.get("price", 0))
            except (TypeError, ValueError):
                price = 0.0

            if not title or not artist:
                self._send_json(400, {"error": "title and artist are required"})
                return

            with self._connect() as conn:
                cur = conn.execute(
                    "INSERT INTO artworks (title, artist, medium, price) VALUES (?, ?, ?, ?)",
                    (title, artist, medium, price),
                )
                new_id = cur.lastrowid
            self._send_json(201, {"id": new_id})

        def do_PUT(self) -> None:
            path = urlparse(self.path).path
            artwork_id = parse_artwork_id(path)
            if artwork_id is None:
                self._send_json(404, {"error": "not found"})
                return

            data = self._read_json()
            if data is None:
                self._send_json(400, {"error": "invalid json"})
                return

            title = str(data.get("title", "")).strip()
            artist = str(data.get("artist", "")).strip()
            medium = str(data.get("medium", "")).strip()
            try:
                price = float(data.get("price", 0))
            except (TypeError, ValueError):
                price = 0.0

            if not title or not artist:
                self._send_json(400, {"error": "title and artist are required"})
                return

            with self._connect() as conn:
                cur = conn.execute(
                    "UPDATE artworks SET title=?, artist=?, medium=?, price=? WHERE id=?",
                    (title, artist, medium, price, artwork_id),
                )
                updated = cur.rowcount
            if updated == 0:
                self._send_json(404, {"error": "update failed"})
            else:
                self._send_json(200, {"status": "updated"})

        def do_DELETE(self) -> None:
            path = urlparse(self.path).path
            artwork_id = parse_artwork_id(path)
            if artwork_id is None:
                self._send_json(404, {"error": "not found"})
                return

            with self._connect() as conn:
                cur = conn.execute("DELETE FROM artworks WHERE id=?", (artwork_id,))
                deleted = cur.rowcount
            if deleted == 0:
                self._send_json(404, {"error": "delete failed"})
            else:
                self._send_json(200, {"status": "deleted"})

    return ExhibitionHandler


def main() -> None:
    parser = argparse.ArgumentParser(description="The Exhibition Python server")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on (default: 8080)")
    parser.add_argument("--db", type=Path, default=DEFAULT_DB, help="Path to SQLite database")
    parser.add_argument("--public", type=Path, default=DEFAULT_PUBLIC, help="Static frontend directory")
    args = parser.parse_args()

    init_db(args.db)
    server = ThreadingHTTPServer(("0.0.0.0", args.port), make_handler(args.db, args.public))
    print(f"Art Exhibition (Python) server running on http://localhost:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
