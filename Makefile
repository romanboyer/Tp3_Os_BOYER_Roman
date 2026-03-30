default : cliudp servudp clibeuip creme.o biceps

cliudp : cliudp.c
	cc -Wall -Werror -o cliudp cliudp.c

servudp : servudp.c
	cc -Wall -Werror -o servudp servudp.c

clibeuip : clibeuip.c
	cc -Wall -Werror -DTRACE -o clibeuip clibeuip.c

creme.o: creme.c creme.h
	cc -Wall -Werror -DTRACE -pthread -c creme.c

biceps : triceps.c creme.o
	cc -Wall -Werror -DTRACE -pthread -o biceps triceps.c creme.o

clean :
	rm -f cliudp servudp clibeuip biceps triceps *.o