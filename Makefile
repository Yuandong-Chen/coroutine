all : main

main : main.c ezco.c
	gcc -m32 -Wall -o $@ $^

clean :
	-rm main
