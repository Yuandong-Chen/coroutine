System := $(shell uname)

all:
	gcc -m32 demo/demo1.c ezco.c -o demo1.out
	gcc -m32 demo/demo2.c ezco.c -o demo2.out
ifeq ($(System),Darwin)
	gcc -m32 demo/demo3.c ezco.c -o demo3.out
	gcc -m32 echoserver.c ezco.c -o echoserver
else
	gcc -m32 demo/demo3.c ezco.c -o demo3.out -lrt
	gcc -m32 echoserver.c ezco.c -lrt -o echoserver
endif
	
achive:
	-rm -rf .*
	cp  -r ./ ../ACHIVE

restore:
	-rm -rf ./*
	cp -r ../ACHIVE ./

asm:
	gcc -m32 -S -fno-stack-protector ezco.c

clean:
	-rm -rf .*
	-rm -rf *.out*
