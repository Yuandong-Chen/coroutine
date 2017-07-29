all : main

main : main.c coroutine.c
	gcc -m64 -Wall -o $@ $^

clean :
	-rm main
