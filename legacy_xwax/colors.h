#ifndef COLORS_H
#define COLORS_H

#include <SDL2/SDL.h>

/* Main UI Colors */
extern const SDL_Color COLOR_BACKGROUND;      /* Black background */
extern const SDL_Color COLOR_TEXT;            /* Main text color */
extern const SDL_Color COLOR_ALERT;           /* Alert/warning color */
extern const SDL_Color COLOR_OK;              /* Success/OK color */
extern const SDL_Color COLOR_ELAPSED;         /* Elapsed time color */
extern const SDL_Color COLOR_CURSOR;          /* Cursor color */
extern const SDL_Color COLOR_SELECTED;        /* Selected item color */
extern const SDL_Color COLOR_DETAIL;          /* Detail text color */
extern const SDL_Color COLOR_NEEDLE;          /* Needle/pointer color */
extern const SDL_Color COLOR_ARTIST;          /* Artist text color */
extern const SDL_Color COLOR_BPM;            /* BPM text color */

/* Initialize all colors - call this at program start */
void init_colors(void);

#endif /* !COLORS_H */ 