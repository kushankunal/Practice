all: mythread.c
	gcc -c mythread.c -o mythread.o
	ar rcs mythread.a mythread.o	
	ar rcs mythread_extra.a mythread.o	