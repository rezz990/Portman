#ifndef PORTMAN_DESKTOP_H
#define PORTMAN_DESKTOP_H
#include "platform.h"
#define PM_MAX_SERVICES 32
/* UTF-8 throughout the ABI. GUI converts only at the Win32 boundary. */
typedef struct {
    char name[65], command[2048], cwd[2048];
    unsigned short port;
    int state; /* 0 stopped, 1 starting, 2 listening, 3 running/no port, 4 failed, 5 port warning */
    unsigned int pid;
    unsigned long long elapsed;
    int exit_code;
} pmd_service;
int pmd_init(const char *directory);
int pmd_count(void);
int pmd_get(int index,pmd_service *out);
int pmd_put(int index,const pmd_service *input);
int pmd_remove(int index);
int pmd_import(const char *filename);
int pmd_export(const char *filename);
int pmd_row_matches(const pm_row *row, const char *query);
int pmd_start(int index);
void pmd_stop(int index);
void pmd_tick(void);
int pmd_running(void);
void pmd_shutdown(void);
const char *pmd_error(void);
int pmd_log(int index,char *out,int capacity);
int pmd_log_path(int index,char *out,int capacity);
int pmd_clear_log(int index);
int pm_desktop_main(void);
#endif
