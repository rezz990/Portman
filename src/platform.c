#define _GNU_SOURCE
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
static volatile LONG interrupted;
static BOOL WINAPI on_ctrl(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        InterlockedExchange(&interrupted, 1); return TRUE;
    }
    return FALSE;
}
void pm_install_signals(void) { SetConsoleCtrlHandler(on_ctrl, TRUE); }
int pm_interrupted(void) { return InterlockedCompareExchange(&interrupted, 0, 0) != 0; }
static uint64_t identity_of(HANDLE h) {
    FILETIME a,b,c,d;
    if (!GetProcessTimes(h,&a,&b,&c,&d)) return 0;
    return ((uint64_t)a.dwHighDateTime << 32) | a.dwLowDateTime;
}
static wchar_t *wide(const char *s) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0);
    if (!n) return NULL;
    wchar_t *w=calloc((size_t)n,sizeof(wchar_t));
    if (w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n);
    return w;
}
static void process_info(pm_row *r) {
    strcpy(r->name,"<unavailable>");
    HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,r->pid);
    if (!h) return;
    r->identity=identity_of(h);
    wchar_t path[32768]; DWORD n=32768;
    if (QueryFullProcessImageNameW(h,0,path,&n)) {
        WideCharToMultiByte(CP_UTF8,0,path,-1,r->executable,sizeof(r->executable),NULL,NULL);
        wchar_t *base=wcsrchr(path,L'\\');
        WideCharToMultiByte(CP_UTF8,0,base?base+1:path,-1,r->name,sizeof(r->name),NULL,NULL);
    }
    CloseHandle(h);
}
int pm_scan(pm_row *rows,size_t cap) {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2,2),&data)) return -1;
    size_t count=0;
    for (int family=0;family<2;family++) {
        ULONG size=0; DWORD rc;
        rc=GetExtendedTcpTable(NULL,&size,FALSE,family?AF_INET6:AF_INET,TCP_TABLE_OWNER_PID_LISTENER,0);
        if (rc!=ERROR_INSUFFICIENT_BUFFER && rc!=NO_ERROR) { WSACleanup(); return -1; }
        void *table=malloc(size?size:4);
        if (!table) { WSACleanup(); return -1; }
        for (int retry=0;;retry++) {
            rc=GetExtendedTcpTable(table,&size,FALSE,family?AF_INET6:AF_INET,TCP_TABLE_OWNER_PID_LISTENER,0);
            if (rc!=ERROR_INSUFFICIENT_BUFFER || retry==3) break;
            void *p=realloc(table,size);
            if (!p) { free(table); WSACleanup(); return -1; }
            table=p;
        }
        if (rc!=NO_ERROR) { free(table); WSACleanup(); return -1; }
        DWORD n=*(DWORD*)table;
        for (DWORD i=0;i<n;i++) {
            if (count>=cap) { free(table); WSACleanup(); return -2; }
            pm_row *r=&rows[count++]; memset(r,0,sizeof(*r));
            r->ipv6=(uint8_t)family;
            if (family) {
                MIB_TCP6ROW_OWNER_PID *t=&((MIB_TCP6TABLE_OWNER_PID*)table)->table[i];
                r->pid=t->dwOwningPid; r->port=ntohs((u_short)t->dwLocalPort);
                InetNtopA(AF_INET6,t->ucLocalAddr,r->address,sizeof(r->address));
                if (t->dwLocalScopeId) {
                    size_t len=strlen(r->address);
                    snprintf(r->address+len,sizeof(r->address)-len,"%%%lu",(unsigned long)t->dwLocalScopeId);
                }
            } else {
                MIB_TCPROW_OWNER_PID *t=&((MIB_TCPTABLE_OWNER_PID*)table)->table[i];
                r->pid=t->dwOwningPid; r->port=ntohs((u_short)t->dwLocalPort);
                InetNtopA(AF_INET,&t->dwLocalAddr,r->address,sizeof(r->address));
            }
            process_info(r);
        }
        free(table);
    }
    WSACleanup(); return (int)count;
}
int pm_terminate(uint32_t pid,uint32_t proc_pid,uint64_t identity,int force) {
    (void)proc_pid;
    if (!force) return -3; /* Windows has no general SIGTERM equivalent. */
    if (pid<=4 || pid==GetCurrentProcessId() || !identity) return -1;
    HANDLE h=OpenProcess(PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if (!h) return -1;
    int ok=identity_of(h)==identity && TerminateProcess(h,1);
    CloseHandle(h); return ok?0:-1;
}
struct pm_child { HANDLE process,job; int done,code; };
pm_child *pm_start(const char *command,const char *cwd,const char *log) {
    pm_child *c=calloc(1,sizeof(*c));
    if (!c) return NULL;
    wchar_t *wd=wide(cwd), *wl=wide(log);
    size_t len=strlen(command)+32;
    char *line=malloc(len);
    if (!line || !wd || !wl) { free(line); free(wd); free(wl); free(c); return NULL; }
    snprintf(line,len,"cmd.exe /d /s /c \"%s\"",command);
    wchar_t *wc=wide(line); free(line);
    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};
    HANDLE out=CreateFileW(wl,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    free(wl);
    HANDLE in=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);
    c->job=CreateJobObjectW(NULL,NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits; memset(&limits,0,sizeof(limits));
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    PROCESS_INFORMATION pi; memset(&pi,0,sizeof(pi));
    STARTUPINFOW si; memset(&si,0,sizeof(si)); si.cb=sizeof(si);
    si.dwFlags=STARTF_USESTDHANDLES; si.hStdInput=in; si.hStdOutput=out; si.hStdError=out;
    wchar_t shell[32768]; UINT sl=GetSystemDirectoryW(shell,32700);
    if (sl && sl<32700) wcscat(shell,L"\\cmd.exe");
    int ok=wc && sl && sl<32700 && out!=INVALID_HANDLE_VALUE && in!=INVALID_HANDLE_VALUE && c->job &&
        SetInformationJobObject(c->job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)) &&
        CreateProcessW(shell,wc,NULL,NULL,TRUE,CREATE_SUSPENDED|CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW,NULL,wd,&si,&pi);
    free(wd); free(wc);
    if (out!=INVALID_HANDLE_VALUE) CloseHandle(out);
    if (in!=INVALID_HANDLE_VALUE) CloseHandle(in);
    if (ok) {
        ok=AssignProcessToJobObject(c->job,pi.hProcess);
        if (ok) ok=ResumeThread(pi.hThread)!=(DWORD)-1;
        if (!ok) TerminateProcess(pi.hProcess,1);
        CloseHandle(pi.hThread);
    }
    if (!ok) { if(pi.hProcess) CloseHandle(pi.hProcess); if(c->job) CloseHandle(c->job); free(c); return NULL; }
    c->process=pi.hProcess; return c;
}
int pm_poll(pm_child *c,int *code) {
    if (!c->done) {
        DWORD result=WaitForSingleObject(c->process,0);
        if(result==WAIT_TIMEOUT) return 0;
        if(result!=WAIT_OBJECT_0) return -1;
        DWORD exit_code;
        if(!GetExitCodeProcess(c->process,&exit_code)) return -1;
        c->code=exit_code>255?1:(int)exit_code; c->done=1;
    }
    *code=c->code; return 1;
}
void pm_stop(pm_child *c) { if(c->job) { CloseHandle(c->job); c->job=NULL; } WaitForSingleObject(c->process,5000); }
void pm_destroy(pm_child *c) { pm_stop(c); CloseHandle(c->process); free(c); }
uint32_t pm_child_pid(pm_child *c) { return GetProcessId(c->process); }
int pm_child_owns(pm_child *c,uint32_t pid) {
    HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!h) return 0;
    BOOL inside=FALSE;
    BOOL ok=IsProcessInJob(h,c->job,&inside);
    CloseHandle(h); return ok && inside;
}
#elif defined(__linux__)
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <time.h>
static volatile sig_atomic_t interrupted;
static void on_signal(int s) { (void)s; interrupted=1; }
void pm_install_signals(void) {
    struct sigaction sa; memset(&sa,0,sizeof(sa)); sa.sa_handler=on_signal;
    sigemptyset(&sa.sa_mask); sigaction(SIGINT,&sa,NULL); sigaction(SIGTERM,&sa,NULL);
}
int pm_interrupted(void) { return interrupted!=0; }
static uint64_t identity_of(uint32_t pid) {
    char path[80],buf[4096]; snprintf(path,sizeof(path),"/proc/%u/stat",pid);
    FILE *f=fopen(path,"r"); if(!f) return 0;
    char *result=fgets(buf,sizeof(buf),f); fclose(f); if(!result) return 0;
    char *p=strrchr(buf,')'); if(!p) return 0;
    char *save=NULL,*word=strtok_r(p+2," ",&save);
    for(int field=3;word;field++,word=strtok_r(NULL," ",&save))
        if(field==22) return strtoull(word,NULL,10);
    return 0;
}
/* /proc may be mounted from an ancestor PID namespace (containers/WSL).
   Expose a PID usable by this process, never accidentally target the host PID. */
static uint32_t local_pid(uint32_t proc_pid) {
    char path[80],own[128],other[128],line[1024];
    ssize_t a=readlink("/proc/self/ns/pid",own,sizeof(own));
    snprintf(path,sizeof(path),"/proc/%u/ns/pid",proc_pid);
    ssize_t b=readlink(path,other,sizeof(other));
    if(a<0 || a!=b || memcmp(own,other,(size_t)a)) return 0;
    snprintf(path,sizeof(path),"/proc/%u/status",proc_pid);
    FILE *f=fopen(path,"r"); if(!f) return 0;
    uint32_t result=0;
    while(fgets(line,sizeof(line),f)) if(!strncmp(line,"NSpid:",6)) {
        char *save=NULL;
        for(char *p=strtok_r(line+6," \t\n",&save);p;p=strtok_r(NULL," \t\n",&save)) result=(uint32_t)strtoul(p,NULL,10);
        break;
    }
    fclose(f); return result;
}
static int scan_table(const char *path,int ipv6,pm_row *rows,size_t cap,size_t *count) {
    FILE *f=fopen(path,"r"); if(!f) return ipv6 && errno==ENOENT?0:-1;
    char line[1024]; if(!fgets(line,sizeof(line),f)) { fclose(f); return -1; }
    while(fgets(line,sizeof(line),f)) {
        char *fields[16],*save=NULL; int n=0;
        for(char *p=strtok_r(line," \t\n",&save);p && n<16;p=strtok_r(NULL," \t\n",&save)) fields[n++]=p;
        if(n<10) { fclose(f); return -1; }
        if(strcmp(fields[3],"0A")) continue;
        if(*count>=cap) { fclose(f); return -2; }
        pm_row *r=&rows[(*count)++]; memset(r,0,sizeof(*r)); r->ipv6=(uint8_t)ipv6;
        char *colon=strchr(fields[1],':'); if(!colon) { fclose(f); return -1; }
        *colon=0; r->port=(uint16_t)strtoul(colon+1,NULL,16); r->inode=strtoull(fields[9],NULL,10);
        uint32_t words[4]={0};
        if(strlen(fields[1])!=(ipv6?32u:8u)) { fclose(f); return -1; }
        for(int i=0;i<(ipv6?4:1);i++) { char hex[9]; memcpy(hex,fields[1]+i*8,8); hex[8]=0; words[i]=(uint32_t)strtoul(hex,NULL,16); }
        inet_ntop(ipv6?AF_INET6:AF_INET,words,r->address,sizeof(r->address));
        strcpy(r->name,"<unavailable>");
    }
    int failed=ferror(f); fclose(f); return failed?-1:0;
}
int pm_scan(pm_row *rows,size_t cap) {
    size_t count=0; int rc=scan_table("/proc/net/tcp",0,rows,cap,&count);
    if(rc) return rc;
    rc=scan_table("/proc/net/tcp6",1,rows,cap,&count); if(rc) return rc;
    DIR *proc=opendir("/proc"); if(!proc) return -1;
    struct dirent *e;
    while((e=readdir(proc))) {
        char *end; unsigned long value=strtoul(e->d_name,&end,10);
        if(!value || *end || value>UINT32_MAX) continue;
        uint32_t pid=(uint32_t)value;
        uint64_t identity=identity_of(pid); if(!identity) continue;
        char path[128]; snprintf(path,sizeof(path),"/proc/%u/fd",pid);
        DIR *fds=opendir(path); if(!fds) continue;
        struct dirent *fd;
        while((fd=readdir(fds))) {
            if(fd->d_name[0]=='.') continue;
            char link[128]; ssize_t n=readlinkat(dirfd(fds),fd->d_name,link,sizeof(link)-1);
            if(n<0) continue;
            link[n]=0; unsigned long long inode;
            if(sscanf(link,"socket:[%llu]",&inode)!=1) continue;
            for(size_t i=0;i<count;i++) if(rows[i].inode==inode && !rows[i].pid) {
                pm_row *r=&rows[i]; r->pid=local_pid(pid); r->proc_pid=pid; r->identity=identity;
                snprintf(path,sizeof(path),"/proc/%u/comm",pid);
                FILE *f=fopen(path,"r");
                if(f) { if(fgets(r->name,sizeof(r->name),f)) r->name[strcspn(r->name,"\r\n")]=0; fclose(f); }
                snprintf(path,sizeof(path),"/proc/%u/exe",pid);
                ssize_t len=readlink(path,r->executable,sizeof(r->executable)-1);
                if(len>=0) r->executable[len]=0;
            }
        }
        closedir(fds);
    }
    closedir(proc); return (int)count;
}
int pm_terminate(uint32_t pid,uint32_t proc_pid,uint64_t identity,int force) {
    if(pid<=4 || pid==(uint32_t)getpid() || !identity) return -1;
    /* pidfd pins identity across PID reuse. No racy kill(pid) fallback. */
    int fd=(int)syscall(SYS_pidfd_open,pid,0);
    if(fd<0) return -1;
    int rc=-1;
    if(local_pid(proc_pid)==pid && identity_of(proc_pid)==identity) rc=(int)syscall(SYS_pidfd_send_signal,fd,force?SIGKILL:SIGTERM,NULL,0);
    close(fd); return rc;
}
struct pm_child { pid_t pid; int done,code; };
pm_child *pm_start(const char *command,const char *cwd,const char *log) {
    int out=open(log,O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC,0600); if(out<0) return NULL;
    int channel[2]; if(pipe2(channel,O_CLOEXEC)) { close(out); return NULL; }
    pm_child *c=calloc(1,sizeof(*c));
    if(!c) { close(out); close(channel[0]); close(channel[1]); return NULL; }
    c->pid=fork();
    if(c->pid==0) {
        close(channel[0]);
        struct sigaction sa; memset(&sa,0,sizeof(sa)); sa.sa_handler=SIG_DFL;
        sigaction(SIGINT,&sa,NULL); sigaction(SIGTERM,&sa,NULL);
        int input=open("/dev/null",O_RDONLY);
        if(setpgid(0,0) || chdir(cwd) || input<0 || dup2(input,0)<0 || dup2(out,1)<0 || dup2(out,2)<0) goto fail;
        close(input); close(out);
        execl("/bin/sh","sh","-c",command,(char*)NULL);
fail:;
        int reason=errno; (void)write(channel[1],&reason,sizeof(reason)); _exit(127);
    }
    close(out); close(channel[1]);
    if(c->pid<0) { close(channel[0]); free(c); return NULL; }
    int reason=0; ssize_t n;
    do { n=read(channel[0],&reason,sizeof(reason)); } while(n<0 && errno==EINTR);
    close(channel[0]);
    if(n!=0) { kill(-c->pid,SIGKILL); kill(c->pid,SIGKILL); waitpid(c->pid,NULL,0); free(c); return NULL; }
    return c;
}
int pm_poll(pm_child *c,int *code) {
    if(!c->done) {
        /* Leave exited leader waitable until cleanup, pinning the process-group id. */
        siginfo_t info; memset(&info,0,sizeof(info));
        if(waitid(P_PID,(id_t)c->pid,&info,WEXITED|WNOHANG|WNOWAIT)) return errno==EINTR?0:-1;
        if(!info.si_pid) return 0;
        c->done=1; c->code=info.si_code==CLD_EXITED?info.si_status:128+info.si_status;
    }
    *code=c->code; return 1;
}
void pm_stop(pm_child *c) {
    if(c->pid<=0) return;
    kill(-c->pid,SIGTERM);
    struct timespec delay={0,100000000};
    /* Give the whole group a bounded grace period, including descendants. */
    for(int i=0;i<10;i++) nanosleep(&delay,NULL);
    kill(-c->pid,SIGKILL);
    while(waitpid(c->pid,NULL,0)<0 && errno==EINTR) {}
    c->pid=0;
}
void pm_destroy(pm_child *c) { pm_stop(c); free(c); }
uint32_t pm_child_pid(pm_child *c) { return (uint32_t)c->pid; }
int pm_child_owns(pm_child *c,uint32_t pid) { return pid && getpgid((pid_t)pid)==c->pid; }
#else
#error "Portman currently supports Windows and Linux only"
#endif
