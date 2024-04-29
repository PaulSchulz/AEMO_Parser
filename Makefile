CC = gcc
CFLAGS = -Wall -g
#CFLAGS =
LIBS = -lcurl -lcjson -lpaho-mqtt3c -lsqlite3 -lpthread

all: aemo

aemo: aemo.o mqtt.o http.o parser.o sqlite3.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

aemo.o: aemo.c
	$(CC) $(CFLAGS) -c aemo.c mqtt.c http.c parser.c sqlite3.c $(LIBS)

clean:
	-rm aemo aemo.o mqtt.o http.o parser.o sqlite3.o
