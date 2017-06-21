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

#define SAVE(r)	  			asm volatile \
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

#define MAXCOROUTINES 100000 //the allowable maximum of threads, you can change it.
#define PTHREAD_STACK_MIN (3*4096)  // must >= 4 Kbytes
enum STATUS {YIELDED, RESUMED, NONE};

typedef struct __thread_table_regs Regs;
struct __thread_table_regs
{
	int _ebp;
	int _esp;
	int _eip;
    int _eflags;
};

typedef struct __ez_thread Thread_t;
struct __ez_thread
{
	Regs regs;
	int tid;
	unsigned int stacktop;
	unsigned int stacksize;
	void *stack;
	void *retval;
    enum STATUS flags;
};

typedef struct __pnode pNode;
struct __pnode
{
	pNode *next;
	pNode *prev;
	Thread_t *data;
};

typedef struct __loopcursor Cursor;
struct __loopcursor
{
	int total;
	pNode *current;
};

void initez();
void switch_to(int);
int threadCreat(Thread_t **, void *(*)(void *), void *);
int threadJoin(Thread_t *, void **);
Thread_t *findThread(Cursor *, int);
void printThread(Thread_t *);
void printLoop(Cursor *);
void destroy(Cursor *);
void endez();

#define ezco_yield(pth, args, cond)      ({ \
                                            pth->stacktop = (unsigned int)args; \
                                            pth->flags = YIELDED; \
                                            while(!(cond) && (pth->flags != RESUMED)) \
                                            { \
                                                switch_to(0); \
                                            } \
                                            pth->flags = RESUMED; \
                                            (void *)pth->stacktop; \
                                         })
                                        

#define ezco_resume(pth, args)    ({ \
                                            while(pth->flags != YIELDED) \
                                            { \
                                                switch_to(0); \
                                            } \
                                            unsigned int ret = pth->stacktop; \
                                            pth->stacktop = args; \
                                            pth->flags = RESUMED; \
                                            (void *)ret; \
                                        })


extern Cursor live;
extern Cursor dead;
extern Thread_t pmain;
