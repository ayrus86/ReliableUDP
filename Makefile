CC = gcc

LIBS = -lresolv -lsocket -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\

LDFLAGS=-lm

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: client server 

client: udpclient.o get_ifi_info_plus.o udp.o rtt.o
	${CC} ${FLAGS} ${LDFLAGS} -o client udpclient.o udp.o rtt.o get_ifi_info_plus.o ${LIBS}

udpclient.o: udpclient.c
	${CC} ${CFLAGS} ${LDFLAGS} -c udpclient.c

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

server: udpserver.o get_ifi_info_plus.o readline.o rtt.o udp.o
	${CC} ${FLAGS} -o server udpserver.o udp.o rtt.o get_ifi_info_plus.o readline.o ${LIBS}

udpserver.o: udpserver.c
	${CC} ${CFLAGS} -c udpserver.c

udp.o: udp.c
	${CC} ${CFLAGS} -c udp.c
	
rtt.o: rtt.c
	${CC} ${CFLAGS} -c rtt.c

# pick up the thread-safe version of readline.c from directory "threads"

readline.o: /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c
	${CC} ${CFLAGS} -c /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c

clean:
	rm client rtt.o udp.o udpclient.o server udpserver.o get_ifi_info_plus.o readline.o
