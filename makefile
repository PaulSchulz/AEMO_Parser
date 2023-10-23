aemo : aemo.o
	cc -Wall -g -o aemo aemo.o mqtt.o http.o parser.o -lcurl -lcjson -lpaho-mqtt3c -pthread

aemo.o : aemo.c
	cc -Wall -g -c aemo.c mqtt.c http.c parser.c

clean :
	rm aemo aemo.o mqtt.o http.o parser.o
