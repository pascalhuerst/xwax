#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#include "library.h"
#include "watcher.h"

static int inotify_fd = -1;
static pthread_t thread;
static volatile int running = 0;
static struct library *library = NULL;
static const char *scan_script = NULL;
static char *watch_path = NULL;

// Hash table to store watch descriptors and their paths
#define HASH_SIZE 128
struct watch_entry {
    int wd;
    char *path;
    struct watch_entry *next;
};
static struct watch_entry *watches[HASH_SIZE];

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

// If PATH_MAX is not defined, define it (some systems might not have it)
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const char* get_event_description(uint32_t mask) {
    static char buf[256];
    buf[0] = '\0';
    
    if (mask & IN_ACCESS) strcat(buf, "IN_ACCESS ");
    if (mask & IN_MODIFY) strcat(buf, "IN_MODIFY ");
    if (mask & IN_ATTRIB) strcat(buf, "IN_ATTRIB ");
    if (mask & IN_CLOSE_WRITE) strcat(buf, "IN_CLOSE_WRITE ");
    if (mask & IN_CLOSE_NOWRITE) strcat(buf, "IN_CLOSE_NOWRITE ");
    if (mask & IN_CREATE) strcat(buf, "IN_CREATE ");
    if (mask & IN_DELETE) strcat(buf, "IN_DELETE ");
    if (mask & IN_DELETE_SELF) strcat(buf, "IN_DELETE_SELF ");
    if (mask & IN_MOVE_SELF) strcat(buf, "IN_MOVE_SELF ");
    if (mask & IN_MOVED_FROM) strcat(buf, "IN_MOVED_FROM ");
    if (mask & IN_MOVED_TO) strcat(buf, "IN_MOVED_TO ");
    if (mask & IN_OPEN) strcat(buf, "IN_OPEN ");
    if (mask & IN_ISDIR) strcat(buf, "IN_ISDIR ");
    if (mask & IN_UNMOUNT) strcat(buf, "IN_UNMOUNT ");
    if (mask & IN_IGNORED) strcat(buf, "IN_IGNORED ");
    
    return buf;
}

static void log_event(const char *path, const char *name, uint32_t mask) {
    char full_path[PATH_MAX];
    const char *type = (mask & IN_ISDIR) ? "Directory" : "File";
    
    if (name && name[0] != '\0') {
        snprintf(full_path, sizeof(full_path), "%s/%s", path, name);
        fprintf(stderr, "Event: %s [%s] path=%s mask=0x%x (%s)\n", 
                type, name, full_path, mask, get_event_description(mask));
    } else {
        fprintf(stderr, "Event: %s path=%s mask=0x%x (%s)\n", 
                type, path, mask, get_event_description(mask));
    }
}

static unsigned int hash_wd(int wd) {
    return (unsigned int)wd % HASH_SIZE;
}

static void add_watch_entry(int wd, const char *path) {
    unsigned int h = hash_wd(wd);
    struct watch_entry *entry = malloc(sizeof(struct watch_entry));
    if (!entry)
        return;
    
    entry->wd = wd;
    entry->path = strdup(path);
    entry->next = watches[h];
    watches[h] = entry;
    fprintf(stderr, "Added watch for: %s\n", path);
}

static void clear_watches(void) {
    for (int i = 0; i < HASH_SIZE; i++) {
        struct watch_entry *entry = watches[i];
        while (entry) {
            struct watch_entry *next = entry->next;
            fprintf(stderr, "Removing watch for: %s\n", entry->path);
            inotify_rm_watch(inotify_fd, entry->wd);
            free(entry->path);
            free(entry);
            entry = next;
        }
        watches[i] = NULL;
    }
}

static const char* find_path_by_wd(int wd) {
    unsigned int h = hash_wd(wd);
    struct watch_entry *entry = watches[h];
    
    while (entry) {
        if (entry->wd == wd)
            return entry->path;
        entry = entry->next;
    }
    return NULL;
}

static int add_watch_recursive(const char *path) {
    DIR *dir;
    struct dirent *entry;
    char full_path[PATH_MAX];
    int wd;
    
    fprintf(stderr, "Adding watch for directory: %s\n", path);
    
    // Add watch for this directory with ALL possible events
    wd = inotify_add_watch(inotify_fd, path,
                          IN_CREATE | IN_DELETE | IN_DELETE_SELF |
                          IN_MODIFY | IN_MOVE_SELF |
                          IN_MOVED_FROM | IN_MOVED_TO |
                          IN_UNMOUNT | IN_CLOSE_WRITE |
                          IN_ATTRIB | IN_ISDIR);
    
    if (wd == -1) {
        fprintf(stderr, "Warning: Could not watch directory %s: %s (errno=%d)\n", 
                path, strerror(errno), errno);
        return 0;
    }
    
    add_watch_entry(wd, path);
    
    // Recursively add watches for subdirectories
    dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Warning: Could not open directory %s: %s (errno=%d)\n",
                path, strerror(errno), errno);
        return 0;
    }
    
    while ((entry = readdir(dir))) {
        struct stat st;
        
        if (entry->d_name[0] == '.') // Skip hidden files and . ..
            continue;
            
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        fprintf(stderr, "Checking path: %s\n", full_path);
        
        if (stat(full_path, &st) == -1) {
            fprintf(stderr, "Warning: Could not stat %s: %s (errno=%d)\n",
                    full_path, strerror(errno), errno);
            continue;
        }
            
        if (S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Found subdirectory: %s\n", full_path);
            add_watch_recursive(full_path);
        }
    }
    
    closedir(dir);
    return 0;
}

static void *watcher_thread(void *arg)
{
    char buffer[BUF_LEN];
    struct pollfd pfd;
    int need_rescan = 0;
    
    fprintf(stderr, "Watcher thread started (thread_id=%lu)\n", pthread_self());
    
    pfd.fd = inotify_fd;
    pfd.events = POLLIN;

    while (running) {
        int ret = poll(&pfd, 1, 1000); // 1 second timeout
        
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "Poll error: %s (errno=%d)\n", strerror(errno), errno);
            break;
        }
        
        if (ret == 0) { // timeout
            if (need_rescan) {
                fprintf(stderr, "Changes detected, rescanning library...\n");
                if (library_import(library, scan_script, watch_path) == -1) {
                    fprintf(stderr, "Library rescan failed\n");
                } else {
                    fprintf(stderr, "Library rescan completed\n");
                }
                need_rescan = 0;
            }
            continue;
        }
            
        if (pfd.revents & POLLIN) {
            fprintf(stderr, "Got inotify event(s)\n");
            
            int length = read(inotify_fd, buffer, BUF_LEN);
            if (length < 0) {
                fprintf(stderr, "Read error: %s (errno=%d)\n", strerror(errno), errno);
                break;
            }
            
            fprintf(stderr, "Read %d bytes of events\n", length);

            int i = 0;
            while (i < length) {
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                const char *path = find_path_by_wd(event->wd);
                
                if (path) {
                    log_event(path, event->name, event->mask);
                    
                    if (event->mask & IN_ISDIR) {
                        char full_path[PATH_MAX];
                        snprintf(full_path, sizeof(full_path), "%s/%s", 
                                path, event->name);
                                
                        if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                            fprintf(stderr, "New directory detected, adding watch: %s\n", full_path);
                            add_watch_recursive(full_path);
                            need_rescan = 1;
                        } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                            fprintf(stderr, "Directory removed: %s\n", full_path);
                            need_rescan = 1;
                        }
                    } else if (event->mask & (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE)) {
                        need_rescan = 1;
                    }
                } else {
                    fprintf(stderr, "Warning: Could not find path for watch descriptor %d\n", event->wd);
                }
                
                i += EVENT_SIZE + event->len;
            }
        } else if (pfd.revents) {
            fprintf(stderr, "Unexpected poll events: 0x%x\n", pfd.revents);
        }
    }
    
    fprintf(stderr, "Watcher thread stopping\n");
    return NULL;
}

int watcher_init(const char *path, struct library *lib, const char *scanner)
{
    fprintf(stderr, "Initializing directory watcher for: %s\n", path);

    // Store parameters
    library = lib;
    scan_script = scanner;
    watch_path = strdup(path);
    if (!watch_path) {
        perror("strdup");
        return -1;
    }

    // Initialize hash table
    for (int i = 0; i < HASH_SIZE; i++)
        watches[i] = NULL;

    // Initialize inotify
    inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd == -1) {
        perror("inotify_init1");
        goto fail_free;
    }

    fprintf(stderr, "Adding recursive watches...\n");

    // Add recursive watches
    if (add_watch_recursive(path) == -1)
        goto fail_inotify;

    // Start watcher thread
    running = 1;
    if (pthread_create(&thread, NULL, watcher_thread, NULL) != 0) {
        perror("pthread_create");
        goto fail_watch;
    }

    fprintf(stderr, "Directory watcher initialized successfully\n");
    return 0;

fail_watch:
    clear_watches();
fail_inotify:
    close(inotify_fd);
fail_free:
    free(watch_path);
    fprintf(stderr, "Failed to initialize directory watcher\n");
    return -1;
}

void watcher_clear(void)
{
    if (running) {
        fprintf(stderr, "Stopping directory watcher...\n");
        running = 0;
        pthread_join(thread, NULL);
        clear_watches();
        close(inotify_fd);
        free(watch_path);
        fprintf(stderr, "Directory watcher stopped\n");
    }
} 