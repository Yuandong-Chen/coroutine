#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "coroutine.h"

#define ENTRY_STACK_MIN    64 //64 bytes
#define PRIVATE     static // more instructive macro
#define PUBLIC  // more instructive macro
/* Hard coded offsets for different registers in jmp_buf structure, 
 * highly related to setjmp.h document, not cross-plateform definition	
 */
#define RBX_F   0
#define RBP_F   1
#define RSP_F   2
#define R13_F   4
#define R14_F   5
#define R15_F   6
#define RIP_F   7
/* we save the entry_func registers' value in entry_regs and its stack's info saved in a global array*/
PRIVATE jmp_buf entry_regs;
PRIVATE jmp_buf hack_tricky; // stand for hack jmp_buf; it's ugly :(
PRIVATE char entry_stack[ENTRY_STACK_MIN];
PRIVATE pthread_once_t initflag = PTHREAD_ONCE_INIT;

/****                       private functions                      ****/
/* a very simple memory allocator, wrappers of malloc and free functions */
PRIVATE void Free(void **ptr)
{
    if(*ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

PRIVATE void *New(unsigned int n)
{
    void *ptr = malloc(n);
    assert(ptr);
    return ptr;
}

/* a very simple coroutine descriptor (behaves like file descriptor) allocator, 
   O(1) to allocate and free a descriptor implemented by a linked list
 */
/* allocate a coroutine descriptor in free list(a free file table) */
PRIVATE int _alloc_file(Sched_t *S, Coroutine *co)
{
    assert(S->freefiles); // assert schedular's free list is not empty
    int freeid = S->freefiles->cid; // fetch the first file entry's coroutine descriptor
    S->freefiles = S->freefiles->next; // let the free list's head point to the next free file entry
    S->files[freeid].next = NULL; // let the allocated file entry's next pointer point to NULL(cuz it's not in free list right now)
    S->files[freeid].op = READY; // let the allocated file entry's op be READY status
    S->files[freeid].co = co; // let its co point to co passed from caller
    return freeid; // return the coroutine descriptor
}

/* free a coroutine descriptor to free list, return 0 or the program crashes*/
PRIVATE int _free_file(Sched_t *S, int cid)
{
    if(S->files[cid].op == NONE) //op should not be NONE, if cid is still unallocated for any coroutine.
        return 0;	//return without doing anything, if it's the case.

    if(S->files[cid].op != DEAD) // if it's not dead, something is going wrong for sure and we cannot restore it.
    {
        fprintf(stderr, "Error: try to free alive coroutine %d\n",cid);
        assert(0);
    }

    S->files[cid].op = NONE; // assign op to NONE and free it.

    if(S->files[cid].co) // if co is not NULL, we free its stack and co
    {
        Free(&(S->files[cid].co->stack));
        Free(&(S->files[cid].co));
    }

    S->files[cid].next = S->freefiles; // insert back into free list(a file entry table).
    S->freefiles = &S->files[cid]; // as you can see here, we put it to the first place in free list
    return 0;
}

/* coroutine stack allocator */
PRIVATE int _renwStk(Sched_t *S, Coroutine *co)
{
    unsigned long *p = co->regs;
    unsigned int size = S->stacktop - p[RSP_F];
    co->stacksize = (unsigned long)size;
    Free(&(co->stack));
    co->stack = New(size);
    return size;
}

PRIVATE void _relsStk(Coroutine *co)
{  
    co->stacksize = 0;
    Free(&(co->stack));
}

/* relay to/from cmain coroutine */
PRIVATE void relay_cmain(Sched_t *S, int from, int to, void *send, int isResume)
{
    unsigned long *p;
    if((S->files[to].op == NONE) || (S->files[to].op == DEAD))
    {
        fprintf(stderr, "Error: from %d goto unknown Coroutine %d status %d\n", from, to, S->files[to].op);
        return;
    }

    S->running = 0; /* we know we will goto main func */
    S->relay = to;

    if(isResume == -1)
        goto _END;

    if(isResume)
        S->files[to].co->stacktop = from;

    
    S->files[from].op = SUSPEND;
    S->files[to].co->recv = send;
    
    if(setjmp(S->files[from].co->regs)){
        S->files[from].op = RUNNING;
        if(from){ 
            _relsStk(S->files[from].co);
        }
        return;
    }

    if(from){
        p = S->files[from].co->regs;
        _renwStk(S, S->files[from].co);
        memcpy(S->files[from].co->stack, p[RSP_F], S->files[from].co->stacksize);
    }

 _END:
    longjmp(S->cmain.regs, 1);
    assert(0);
}

PRIVATE void cmain_relay(Sched_t *S, int to, void *send, int isResume)
{
    unsigned long *p;
    if((S->files[to].op == NONE) || (S->files[to].op == DEAD))
    {
        fprintf(stderr, "Error: cmain goto unknown Coroutine %d status %d\n", to, S->files[to].op);
        return;
    }

    S->running = to; /* we know we will goto main func */
    S->relay = 0;

    if(isResume)
        S->files[to].co->stacktop = 0;

    
    S->files[0].op = SUSPEND;
    S->files[to].co->recv = send;
    /* save current context */
    
    if(setjmp(S->files[0].co->regs)){
        S->files[0].op = RUNNING;
        return;
    }

    if(to){
        p = S->files[to].co->regs;
        memcpy(p[RSP_F], S->files[to].co->stack,S->files[to].co->stacksize);
    }

    longjmp(S->files[to].co->regs, 1);
    assert(0);
}

/* every coroutine start from here except the cmain coroutine in posix thread(including main thread) */
PRIVATE void entry_func(void *(*start_rtn)(Sched_t*, void *), Sched_t *S, void* args)
{
    if(setjmp(entry_regs) == 0)     //first enter in init_entry function, params are not available.
        longjmp(hack_tricky,1);

    void *send = (*start_rtn)(S, args);
    int from = S->running;
    int to = S->files[S->running].co->stacktop;
    S->files[S->running].op = DEAD;
    _free_file(S, S->running);
    relay_cmain(S, from, to, send, -1);
}

PRIVATE void init_entry()
{
    unsigned long *p;
    bzero(hack_tricky, sizeof(jmp_buf));
    bzero(entry_regs, sizeof(jmp_buf));
    p = entry_regs;
    p[RIP_F] = entry_func;
    p[RBP_F] = p[RSP_F] = ((unsigned long)entry_stack + ENTRY_STACK_MIN)&((long)-16);
    if(setjmp(hack_tricky) == 0)
        longjmp(entry_regs,1);

    bzero(entry_stack, ENTRY_STACK_MIN); // we don't need the entry_stack info right now.
}

/****                       public functions                      ****/
/* allocate a schedular and do some initialization */
PUBLIC Sched_t* coroutine_open(void)
{
    Sched_t *S = New(sizeof(Sched_t));
    /* init our free list(a coroutine file entry table) */
    int i;
    for (i = 0; i < MAXCOROUTINES - 1; ++i)
    {
        S->files[i].cid = i;
        S->files[i].co = NULL;
        S->files[i].op = NONE;
        S->files[i].next = &(S->files[i+1]);
    }

    S->files[i].cid = i;
    S->files[i].co = NULL;
    S->files[i].op = NONE;
    S->files[i].next = NULL;
    S->freefiles = &(S->files[0]);
    S->running = 0;
    S->relay = 0;
    void *sharedStack = S->stack.stk;
    S->stacktop = ((unsigned long)sharedStack + STACK_MIN)&((long)-16);
    S->stacksize = STACK_MIN - (((unsigned long)sharedStack + STACK_MIN) - S->stacktop);
    assert(_alloc_file(S, &S->cmain) == 0); // allocate a coroutine descriptor for the caller thread

    pthread_once(&initflag, init_entry); // make sure this snippets is called only one time.

    return S;
}

/* free a schedular */
PUBLIC void coroutine_close(Sched_t **SC) {
    Sched_t *S = *SC;
    int i;
    for (i = 1; i < MAXCOROUTINES; ++i)
    {
        
        if(S->files[i].op && S->files[i].co){
            Free(&(S->files[i].co->stack));
            Free(&(S->files[i].co));
            S->files[i].co = NULL;
        }
    }
    S->freefiles = NULL;
    S->running = -1;
    
    Free(SC);
}

/* allocate a coroutine */
PUBLIC int coroutine_new(Sched_t *S, void *(*start_rtn)(Sched_t *, void *), void *arg) {
    Coroutine *co = New(sizeof(Coroutine));
    unsigned long *p, *q, diff;
    p = entry_regs;
    q = co->regs;
    memcpy(q, p, sizeof(jmp_buf));
    diff = q[RBP_F] - q[RSP_F];
    co->stack = New(diff + 32);
    //bzero(co->stack, diff + 32);
    co->stacktop = (((unsigned long)co->stack + diff + 32));
    co->stacksize = diff + 32;
    *(unsigned long *)(co->stacktop - 40) = start_rtn; // Hard coded injection -O0 
    *(unsigned long *)(co->stacktop - 48) = S; // Hard coded injection -O0 
    *(unsigned long *)(co->stacktop - 56) = arg;  // Hard coded injection -O0 
    q[RBP_F] = S->stacktop - 32;
    q[RSP_F] = q[RBP_F] - diff;
    q[RBX_F] = S; // Hard coded injection -O1 
    q[R13_F] = S; // Hard coded injection -O2,3 
    q[R15_F] = start_rtn; // Hard coded injection -O1,2,3 
    q[R14_F] = arg; // Hard coded injection -O1,2,3 
    co->stacktop = 0; //clear stacktop info for other use
    return (_alloc_file(S, co));
}


PUBLIC int coroutine_resume(Sched_t *S, int cid, void *chl) {
    if(cid == S->running)
        return chl;
    
    if(S->running)
    {
        relay_cmain(S, S->running, cid, chl, 1);
    }
    else
    {
        cmain_relay(S, cid, chl, 1);
        while(S->relay)
        {
            void *rchl = S->files[S->relay].co->recv;
            cmain_relay(S, S->relay, rchl, 0);
        }
    }

    return S->files[S->running].co->recv;
}

PUBLIC int coroutine_yield(Sched_t *S, void *chl){

    if(S->running)
    {
        relay_cmain(S, S->running, S->files[S->running].co->stacktop, chl, 0);
    }
    else
    {
        cmain_relay(S, S->files[0].co->stacktop, chl, 0);
        while(S->relay)
        {
            void *rchl = S->files[S->relay].co->recv;
            cmain_relay(S, S->relay, rchl, 0);
        }
    }

    return S->files[S->running].co->recv;
}

PUBLIC int coroutine_status(Sched_t *S, int cid){
    enum STATUS ret = S->files[cid].op;
    if((ret == READY) || (ret == SUSPEND) || (ret == RUNNING))
        return ret;

    return 0;
}

PUBLIC int coroutine_running(Sched_t *S) {
    return S->running;
}
