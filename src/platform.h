#ifndef PORTMAN_PLATFORM_H
#define PORTMAN_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t pid;
    uint32_t proc_pid;
    uint16_t port;
    uint8_t ipv6;
    uint64_t inode;
    uint64_t identity;
    char address[64];
    char name[260];
    char executable[1024];
} pm_row;
/* Return row count; negative means failure (never an empty success). */
int pm_scan(pm_row *rows, size_t capacity);
/* Open a stable process reference and verify creation identity before killing. */
int pm_terminate(uint32_t pid, uint32_t proc_pid, uint64_t identity, int force);
void pm_install_signals(void);
int pm_interrupted(void);
typedef struct pm_child pm_child;
/* Each child owns a process group (Linux) or kill-on-close Job (Windows). */
pm_child *pm_start(const char *command, const char *cwd, const char *log);
/* 0 running, 1 exited with code, -1 failure */
int pm_poll(pm_child *child, int *code);
void pm_stop(pm_child *child);
void pm_destroy(pm_child *child);
uint32_t pm_child_pid(pm_child *child);
int pm_child_owns(pm_child *child, uint32_t pid);
#endif
