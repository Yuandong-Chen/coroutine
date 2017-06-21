/************************* Test *************************/

#include <stdio.h>
#include <stdlib.h>
#include "../ezco.h"

#define NUM 3
#define CAL 3

void hi()
{
    switch_to(0);
}

void *sum1tod(void *d)
{
    int i, j=0;

    for (i = 1; i <= d; ++i)
    {
        j += i;
        printf("thread %d is crunching... %d\n",live.current->data->tid , i);
        hi(); // Give up control to next thread
    }

    return ((void *)j);
}

int main(int argc, char const *argv[])
{
    int res = 0;
    int i;
    initez();
    Thread_t *tid1[NUM];
    int *res1[NUM];
    for(i = 0; i<NUM; i++){
    threadCreat(&tid1[i], sum1tod, CAL);
    }
    
    

    for (i = 1; i <= CAL; ++i){
        res+=i;
        switch_to(0); //Give up control to next thread
        printf("main is crunching... %d\n", i);
    }

    for(i = 0; i<NUM; i++){
        threadJoin(tid1[i], &res1[i]); //Collect and Release the resourse of tid1
        res += (int)res1[i];
    }
    
    printf("parallel compute total: %d\n", (int)res);
    endez();
    return 0;
}
