#pragma once
#define JMP(r)	asm volatile \
    			(   \
                    "pushl %3\n\t" \
                    "popf\n\t" \
                    "movl %0, %%esp\n\t" \
        			"movl %2, %%ebp\n\t" \
        			"jmp *%1\n\t" \
        			: \
        			: "m"(r._esp),"a"(r._eip),"m"(r._ebp), "m"(r._eflags) \
        			:  \
    			)

#define SAVE(r)             asm volatile \
                            (  \
                                "movl %%esp, %0\n\t" \
                                "movl %%ebp, %1\n\t" \
                                "pushf\n\t" \
                                "movl (%%esp), %%eax\n\t" \
                                "movl %%eax, %2\n\t" \
                                "popf\n\t" \
                                : "=m"(r._esp),"=m"(r._ebp), "=m"(r._eflags) \
                                : \
                                :  \
                            )

#define ALIGN()             asm volatile \
                            ( \
                                "andl $-16, %%esp\n\t" \
                                : \
                                : \
                                :"%esp" \
                            )

#define MAXCOROUTINES 1000000 //the allowable maximum of threads, you can change it.
#define PTHREAD_STACK_MIN (4096*32)  // 128 Kbytes
enum STATUS {NONE, READY, SUSPEND, RUNNING, DEAD};

typedef struct __regs Regs;
struct __regs
{
	int _ebp;
	int _esp;
	int _eip;
    int _eflags;
} __attribute__((aligned(8)));

typedef struct __co Coroutine;
struct __co
{
	Regs regs;
	unsigned int stacktop;
	unsigned int stacksize;
	void *stack;
	void *recv;
};

typedef struct __file Cofile;
struct __file
{
    int cid;
    Coroutine *co;
    Cofile *next;
    enum STATUS op;
};

typedef struct __stack Stack_t;
struct __stack
{
    char stk[PTHREAD_STACK_MIN];
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
    unsigned int stacktop;
    unsigned int stacksize;
};

Sched_t* coroutine_open(void);
void coroutine_close(Sched_t **); 
int coroutine_new(Sched_t *, void *(*start_rtn)(Sched_t *, void *), void *);
int coroutine_resume(Sched_t *, int, void *);
int coroutine_status(Sched_t *, int);
int coroutine_running(Sched_t *);
int coroutine_yield(Sched_t *, void *);
