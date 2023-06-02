all:
	gcc -Wall -c common.c
	gcc -Wall equipment.c common.o -o equipment
	gcc -Wall server.c common.o -lpthread -o server