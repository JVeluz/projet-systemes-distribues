all: main display

main: main.c
	gcc -o main.o main.c -lpthread


display: display.c
	gcc -o display.o display.c

clean:
	rm -f *.o pipe