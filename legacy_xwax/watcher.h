#ifndef WATCHER_H
#define WATCHER_H

#include "library.h"

int watcher_init(const char *path, struct library *lib, const char *scanner);
void watcher_clear(void);
int library_remove_by_path(struct library *lib, const char *path);

#endif 