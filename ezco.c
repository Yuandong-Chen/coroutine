#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ezco.h"

void real_entry(void *(*start_rtn)(Sched_t *, void *), Sched_t *, void*);
void relay_cmain(Sched_t *, int, int, void *, int);

static void Free(void **ptr)
{
    if(*ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

static void *New(unsigned int n)
{
    void *ptr = malloc(n);
    assert(ptr);
    return ptr;
}

int _alloc_file(Sched_t *S, Coroutine *co)
{
    assert(S->freefiles);
    int freeid = S->freefiles->cid;
    S->freefiles = S->freefiles->next;
    S->files[freeid].next = NULL;
    S->files[freeid].op = READY;
    S->files[freeid].co = co;
    return freeid;
}

int _free_file(Sched_t *S, int cid)
{
    if(S->files[cid].op == NONE)
        return 0;

    if(S->files[cid].op != DEAD)
    {
        fprintf(stderr, "try to free alive coroutine %d\n",cid);
        assert(0);
    }

    S->files[cid].op = NONE;

    if(S->files[cid].co)
    {
        Free(&(S->files[cid].co->stack));
        Free(&(S->files[cid].co));
    }

    S->files[cid].next = S->freefiles;
    S->freefiles = &S->files[cid];
    return 0;
}

Sched_t* coroutine_open(void)
{
    Sched_t *S = New(sizeof(Sched_t));
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
    S->stacktop = (((int)sharedStack + PTHREAD_STACK_MIN)&(0xfffffff0));
    S->stacksize = PTHREAD_STACK_MIN - (((int)sharedStack + PTHREAD_STACK_MIN) - S->stacktop);
    assert(_alloc_file(S, &S->cmain) == 0);
    return S;
}

void coroutine_close(Sched_t **SC) {
    Sched_t *S = *SC;
    int i;
    for (i = 1; i < MAXCOROUTINES - 1; ++i)
    {
        
        if(S->files[i].op && S->files[i].co){
            Free(&(S->files[i].co->stack));
            Free(&(S->files[i].co));
            S->files[i].co = NULL;
        }
        S->freefiles = NULL;
        S->running = -1;
    }

    Free(SC);
}

int coroutine_new(Sched_t *S, void *(*start_rtn)(Sched_t *, void *), void *arg) {
    Coroutine *co = New(sizeof(Coroutine));
    co->stack = New(16);
    co->stacktop = (((int)co->stack + 16));
    co->stacksize = 16;
    void *dest = co->stacktop - 12;
    memcpy(dest, &start_rtn, 4);
    dest += 4;
    memcpy(dest, &S, 4);
    dest += 4;
    memcpy(dest, &arg, 4);
    co->regs._eip = &real_entry;
    co->regs._esp = S->stacktop - 16;
    co->regs._ebp = 0;
    co->regs._eflags = 0;
    co->stacktop = 0;
    return (_alloc_file(S, co));
}

void real_entry(void *(*start_rtn)(Sched_t*, void *), Sched_t *S, void* args)
{
    ALIGN();

    void *send = (*start_rtn)(S, args);
    int from = S->running;
    int to = S->files[S->running].co->stacktop;
    S->files[S->running].op = DEAD;
    _free_file(S, S->running);
    relay_cmain(S, from, to, send, -1);
}

int _renwStk(Sched_t *S, Coroutine *co)
{
    int size = S->stacktop - co->regs._esp;
    co->stacksize = size;
    Free(&(co->stack));
    co->stack = New(size);
    return size;
}

void _relsStk(Coroutine *co)
{  
    co->stacksize = 0;
    Free(&(co->stack));
}

void relay_cmain(Sched_t *S, int from, int to, void *send, int isResume)
{
    /* checking real destination's validation */
    //assert(from);
    if((S->files[to].op == NONE) || (S->files[to].op == DEAD))
    {
        fprintf(stderr, "from %d goto unknown Coroutine %d status %d\n", from, to, S->files[to].op);
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
    /* save current context */
    S->files[from].co->regs._eip = &&_REENTERPOINT;
    SAVE(S->files[from].co->regs);

    if(from){
        _renwStk(S, S->files[from].co);
        memcpy(S->files[from].co->stack, S->files[from].co->regs._esp, S->files[from].co->stacksize);
    }

    if(isResume == -1){
 _REENTERPOINT:
        S->files[from].op = RUNNING;
        if(from){ 
            _relsStk(S->files[from].co);
        }
        return;
    }

 _END:
    JMP(S->cmain.regs);
    assert(0);
}

void cmain_relay(Sched_t *S, int to, void *send, int isResume)
{
    /* checking real destination's validation */
    //assert(from);
    if((S->files[to].op == NONE) || (S->files[to].op == DEAD))
    {
        fprintf(stderr, "cmain goto unknown Coroutine %d status %d\n", to, S->files[to].op);
        return;
    }

    S->running = to; /* we know we will goto main func */
    S->relay = 0;

    if(isResume)
        S->files[to].co->stacktop = 0;

    
    S->files[0].op = SUSPEND;
    S->files[to].co->recv = send;
    /* save current context */
    S->files[0].co->regs._eip = &&_REENTERPOINT;
    SAVE(S->files[0].co->regs);


    if(isResume == -1){
 _REENTERPOINT:
        S->files[0].op = RUNNING;
        return;
    }

    if(to){
        memcpy(S->files[to].co->regs._esp, S->files[to].co->stack,S->files[to].co->stacksize);
    }
   
    JMP(S->files[to].co->regs);
    assert(0);
}

int coroutine_resume(Sched_t *S, int cid, void *chl) {
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

int coroutine_yield(Sched_t *S, void *chl){

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

int coroutine_status(Sched_t *S, int cid){
    enum STATUS ret = S->files[cid].op;
    if((ret == READY) || (ret == SUSPEND) || (ret == RUNNING))
        return ret;

    return 0;
}

int coroutine_running(Sched_t *S) {
    return S->running;
}
