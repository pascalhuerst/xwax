#include "colors.h"

/* Define all colors with their RGB values */
const SDL_Color COLOR_BACKGROUND = {0, 0, 0, 255};          /* Black */
const SDL_Color COLOR_TEXT = {224, 224, 224, 255};         /* Light gray */
const SDL_Color COLOR_ALERT = {192, 64, 0, 255};          /* Orange-red */
const SDL_Color COLOR_OK = {32, 128, 3, 255};            /* Green */
const SDL_Color COLOR_ELAPSED = {0, 32, 255, 255};        /* Blue */
const SDL_Color COLOR_CURSOR = {192, 0, 0, 255};          /* Red */
const SDL_Color COLOR_SELECTED = {0, 48, 64, 255};        /* Dark blue-green */
const SDL_Color COLOR_DETAIL = {128, 128, 128, 255};      /* Gray */
const SDL_Color COLOR_NEEDLE = {255, 255, 255, 255};      /* White */
const SDL_Color COLOR_ARTIST = {16, 64, 0, 255};         /* Dark green */
const SDL_Color COLOR_BPM = {64, 16, 0, 255};           /* Dark red */

/* No initialization needed since we use const values */
void init_colors(void) {
    /* Nothing to do here since we use const values */
    return;
} 