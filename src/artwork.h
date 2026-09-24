#ifndef ARTWORK_H
#define ARTWORK_H

/* Core struct representing one artwork in the exhibition.
   "medium" holds the type of art: "Watercolor", "Acrylic",
   "Sketch", "Pen Sketch", "Oil", "Digital", etc. */
typedef struct {
    int    id;
    char   title[100];
    char   artist[100];
    char   medium[50];
    double price;
} Artwork;

#endif
