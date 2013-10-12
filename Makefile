CC = gcc

LIBS = -lresolv -lsocket -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\
	
FLAGS = -g -O2

CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: client server 


client: udpclient.o
	${CC} ${FLAGS} -o client udpclient.o get_ifi_info_plus.o ${LIBS}
udpclient.o: udpclient.c
	${CC} ${CFLAGS} -c udpclient.c


server: udpserver.o get_ifi_info_plus.o readline.o
	${CC} ${FLAGS} -o server udpserver.o get_ifi_info_plus.o readline.o ${LIBS}
udpserver.o: udpserver.c
	${CC} ${CFLAGS} -c udpserver.c

# pick up the thread-safe version of readline.c from directory "threads"

readline.o: /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c
	${CC} ${CFLAGS} -c /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c
clean:
	rm client udpclient.o server udpserver.o get_ifi_info_plus.o readline.o
