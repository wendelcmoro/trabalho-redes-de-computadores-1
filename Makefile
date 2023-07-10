CC = gcc
CFLAGS = -pthread

OBJ1 = client.o aux.o
OBJ2 = server.o aux.o

all: client server clean

client: $(OBJ1)
	$(CC) $(CFLAGS) -o client $(OBJ1)

server: $(OBJ2)
	$(CC) $(CFLAGS) -o server $(OBJ2)

clean: 
	rm -rf *.o