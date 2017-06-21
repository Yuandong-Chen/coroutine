#include "ezco.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

Cursor live;
Cursor dead;
Thread_t pmain;

static inline void Free(void **ptr)
{
    if(*ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

static inline void *New(unsigned int n)
{
    void *ptr = malloc(n);
    assert(ptr);
    return ptr;
}

void initCursor(Cursor *cur)
{
	cur->total = 0;
	cur->current = NULL;
}

Thread_t *findThread(Cursor *cur, int tid)
{
    int counter = cur->total;
    if(counter == 0){
        return NULL;
    }

    int i;
    pNode *tmp = cur->current;
    for (i = 0; i < counter; ++i)
    {
        if((tmp->data)->tid == tid){
            return tmp->data;
        }

        tmp = tmp->next;
    }
    return NULL;
}

int appendThread(Cursor *cur, Thread_t *pth)
{
	if(cur->total == 0)
	{
		cur->current = (pNode *)New(sizeof(pNode));
		(cur->current)->data = pth;
		(cur->current)->prev = cur->current;
		(cur->current)->next = cur->current;
		cur->total++;
		return 0;
	}
	else
	{	
		if(cur->total > MAXCOROUTINES)
		{
			assert((cur->total == MAXCOROUTINES));
			return -1;
		}
        
		pNode *tmp = New(sizeof(pNode));
		tmp->data = pth;
		tmp->prev = cur->current;
		tmp->next = (cur->current)->next;
		((cur->current)->next)->prev = tmp;
		(cur->current)->next = tmp;
		cur->total++;
		return 0;
	}
}

pNode *deleteThread(Cursor *cur, int tid)
{
	int counter = cur->total;
	int i;
	pNode *tmp = cur->current;
	for (i = 0; i < counter; ++i)
	{
		if((tmp->data)->tid == tid){
			(tmp->prev)->next = tmp->next;
			(tmp->next)->prev = tmp->prev;
            if(tmp == cur->current)
            {
                cur->current = cur->current->next;
            }  

			cur->total--;
            assert(cur->total >= 0);
			return tmp;
		}
		tmp = tmp->next;
	}
    return NULL;
}

int fetchTID()
{
	static int tid;
	return ++tid;
}

void real_entry(Thread_t *pth, void *(*start_rtn)(void *), void* args)
{
    ALIGN();

    pth->retval = (*start_rtn)(args);

    //deleteThread(&live, pth->tid);
    pNode *tmp = deleteThread(&live, pth->tid);
    Free(&tmp);
    appendThread(&dead, pth);

    switch_to(-1);
}

int threadCreat(Thread_t **pth, void *(*start_rtn)(void *), void *arg)
{

    *pth = New(sizeof(Thread_t));
    (*pth)->stack = New(PTHREAD_STACK_MIN);
    assert((*pth)->stack);
    (*pth)->stacktop = (((int)(*pth)->stack + PTHREAD_STACK_MIN)&(0xfffffff0));
    (*pth)->stacksize = PTHREAD_STACK_MIN - (((int)(*pth)->stack + PTHREAD_STACK_MIN) - (*pth)->stacktop);
    (*pth)->tid = fetchTID();
    /* set params */
    void *dest = (*pth)->stacktop - 12;
    memcpy(dest, pth, 4);
    dest += 4;
    memcpy(dest, &start_rtn, 4);
    dest += 4;
    memcpy(dest, &arg, 4);
    (*pth)->regs._eip = &real_entry;
    (*pth)->regs._esp = (*pth)->stacktop - 16;
    (*pth)->regs._ebp = 0;
    (*pth)->flags = NONE;
    appendThread(&live, (*pth));

    return 0;
}

int threadJoin(Thread_t *pth, void **rval_ptr)
{

    Thread_t *find1, *find2;
    find1 = findThread(&live, pth->tid);
    find2 = findThread(&dead, pth->tid);
    

    if((find1 == NULL)&&(find2 == NULL)){
        
        return -1;
    }

    if(find2){
        if(rval_ptr != NULL)
            *rval_ptr = find2->retval;

        pNode *tmp = deleteThread(&dead, pth->tid);
        Free(&tmp);
        Free(&find2->stack);
        Free(&find2);
        return 0;
    }

    while(1)
    {
        switch_to(0);
        if((find2 = findThread(&dead, pth->tid))!= NULL){
            if(rval_ptr!= NULL)
                *rval_ptr = find2->retval;

            pNode *tmp = deleteThread(&dead, pth->tid);
            Free(&tmp);
            Free(&find2->stack);
            Free(&find2);
            return 0;
        }   
    }
    return -1;
}

void initez()
{
	initCursor(&live);
	initCursor(&dead);
	appendThread(&live, &pmain);
}

void nothing()
{

}

void switch_to(int signo)
{
    Regs regs;

    if(signo == -1)
    {
        goto _END;
    }
    /* save current context */
    live.current->data->regs._eip = &&_REENTERPOINT;
    SAVE(live.current->data->regs);
    
    if(signo == -1){
 _REENTERPOINT:
        nothing();
        return;
    }
    live.current = live.current->next;
 _END:
    regs = live.current->data->regs;
    JMP(regs);
}

void printThread(Thread_t *pth)
{
    int i;
    printf("pth tid: %d\n", pth->tid);
    printf("pth stack top: %x\n", pth->stacktop);
    printf("pth stack size: %u\n", pth->stacksize);
    printf("pth retval: %p\n", pth->retval);
    printf("EIP: %x\t", pth->regs._eip);
    printf("ESP: %x\t", pth->regs._esp);
    printf("EBP: %x\n", pth->regs._ebp);
    printf("=======Stack Element======\n");

    for (i = 0; i < pth->stacksize/4; ++i)
    {
        printf("%x\n", *(int *)(pth->stack + 4*i));
    }

    printf("=======Stack Element======\n");
}

void printLoop(Cursor *cur)
{
    int count = 0;
    if(!cur->total)
        return;

    pNode *tmp = cur->current;
    assert(tmp);
    do{
        printThread(tmp->data);
        tmp = tmp->next;
        count ++;
    }while(tmp != cur->current);
    printf("real total: %d\n", count);
    printf("total record:%d\n", cur->total);
    assert(count == cur->total);
}

void destroy(Cursor *cur)
{
    if(!cur->total)
        return;

    pNode *tmp = cur->current;
    pNode *todel = NULL;
    assert(tmp);
    do{
        todel = tmp;
        tmp = tmp->next;

        if(todel->data->tid == 0)
            continue;

        Free(&todel->data->stack);
        Free(&todel->data);
        Free(&todel);
    }while(tmp != cur->current);
    cur->current = NULL;
    cur->total = 0;
}

void endez()
{
    destroy(&dead);
    destroy(&live);
}



