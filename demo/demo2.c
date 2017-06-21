
/************************* Test *************************/

#include <stdio.h>
#include <stdlib.h>
#include "../ezco.h"

#define NUM 6
#define CAL 2
Thread_t *tid[2];
int *res1[2];

void *produceFunc(void *d)
{
    int j=1;
    while(1)
    {
	j = (int)ezco_yield(tid[1],j,0);
	printf("+ %u\n", j);
    }
    
    return ((void *)j);
}

void *consumeFunc(void *d)
{
    int j=0;
    int i=0;
    Thread_t *t = d;
    while(1)
    {
	j += ezco_resume(t,j);
	printf("Fibonacci sequence[%d]: %u\n",i++, j);
    }
    
    return ((void *)j);
}

int main(int argc, char const *argv[])
{
    int i = 0;
    initez();

    threadCreat(&tid[1],produceFunc,NULL);
    threadCreat(&tid[0],consumeFunc,tid[1]);
    
    printf("Compute %d Fibonacci sequences ...(total <=47) \n", NUM - 1);
    for (int i = 0; i < NUM; ++i)
    {
        switch_to(0);
    } 
    endez();
    return 0;
}
