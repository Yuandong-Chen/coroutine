#include <stdio.h>
#include "coroutine.h"

struct args {
	int n;
};

static void *
foo(Sched_t *S, void *ud) {
	struct args * arg = ud;
	int start = arg->n;
	int i;
	for (i=0;i<5;i++) {
		printf("coroutine %d : %d\n",coroutine_running(S) , start + i);
		coroutine_yield(S,NULL);
	}
	return NULL;
}

static void
test(Sched_t *S) {
	struct args arg1 = { 0 };
	struct args arg2 = { 100 };

	int co1 = coroutine_new(S, foo, &arg1);
	int co2 = coroutine_new(S, foo, &arg2);
	printf("main start\n");
	while (coroutine_status(S,co1) && coroutine_status(S,co2)) {
		coroutine_resume(S,co1,NULL);
		coroutine_resume(S,co2,NULL);
	} 
	printf("main end\n");
}

int 
main() {
	Sched_t* S = coroutine_open();
	test(S);
	coroutine_close(&S);
	return 0;
}