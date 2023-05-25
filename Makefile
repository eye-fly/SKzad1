CC = g++
CFLAGS = -Wall -pthread -O2
SRC = sikradio-receiver.cc sikradio-sender.cc
OBJ = $(SRC:.cc=.o)
PROGRAMS = sikradio-receiver sikradio-sender         	

all: $(PROGRAMS)
.PHONY: all 

# %: %.o $(SRC)
# 	$(CC) $(CFLAGS) $^ -o $@

%: %.cc
	$(CC) $(CFLAGS) $@.cc -o $@

# %.cc: $(SRC) util.h
# 	$(CC) $(CFLAGS) $@.cc -o $@

clean:
	rm -f *.o $(PROGRAMS)