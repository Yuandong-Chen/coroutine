#pragma once
#include <setjmp.h>

#define MAXCOROUTINES 10000 //the allowable maximum of threads, you can change it.
#define STACK_MIN (4096*32)  // 128 Kbytes

typedef enum STATUS Status;
enum STATUS {NONE, READY, SUSPEND, RUNNING, DEAD};

typedef struct __co Coroutine;
struct __co
{
	jmp_buf regs;
	unsigned long stacktop;
	unsigned long stacksize;
	void *stack;
	void *recv;
};

typedef struct __file Cofile;
struct __file
{
    int cid;
    Coroutine *co;
    Cofile *next;
    Status op;
};

typedef struct __stack Stack_t;
struct __stack
{
    char stk[STACK_MIN];
} __attribute__((aligned(16))); 

typedef struct __schedular Sched_t;
struct __schedular
{
    Stack_t stack;
    Cofile files[MAXCOROUTINES];
    Cofile *freefiles;
    Coroutine cmain;
    int running;
    int relay;
    unsigned long stacktop;
    unsigned long stacksize;
};

Sched_t* coroutine_open(void);
void coroutine_close(Sched_t **); 
int coroutine_new(Sched_t *, void *(*start_rtn)(Sched_t *, void *), void *);
int coroutine_resume(Sched_t *, int, void *);
int coroutine_status(Sched_t *, int);
int coroutine_running(Sched_t *);
int coroutine_yield(Sched_t *, void *);
