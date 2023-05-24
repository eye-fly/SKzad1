CC = g++
CFLAGS = -Wall -pthread -O2
SRC = sikradio-receiver.c sikradio-sender.c
OBJ = $(SRC:.c=.o)
PROGRAMS = sikradio-receiver sikradio-sender         	

all: $(PROGRAMS)

%: %.o $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(PROGRAMS)