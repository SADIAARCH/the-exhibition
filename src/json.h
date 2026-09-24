#ifndef JSON_H
#define JSON_H

#include "artwork.h"

/* Serializes an array of artworks into a JSON array string. Caller frees result. */
char *json_from_artworks(Artwork *list, int count);

/* Serializes aggregated stats (counts + totals per medium) into JSON. Caller frees. */
char *json_stats_from_artworks(Artwork *list, int count);

/* Parses a flat JSON object body ({"title":"...", "artist":"...",
   "medium":"...", "price": 12.5}) into an Artwork. Missing fields left blank/0.
   Returns 0 on success. Only handles the simple flat shape our own frontend sends. */
int json_parse_artwork(const char *body, Artwork *out);

/* Escapes a string for safe embedding inside a JSON string literal. Caller frees. */
char *json_escape(const char *s);

#endif
