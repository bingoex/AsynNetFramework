

DEBUG= -g

CSRC=$(shell ls *.c)
CPPSRC=$(shell ls *.cpp)

OBJS=$(CSRC:.c=.o) 
#OBJS+= $(SPPSRC:.cpp=.o)

ARGS = -g -Wall -fno-strict-aliasing

CC = g++

.c.o:
	$(CC) $(ARGS) -c $*.c

all: $(OBJS)
	rm -f amf.a
	ar -q amf.a $(OBJS)

print:
	@echo $(CSRC)
	@echo $(CPPSRC)
	@echo $(OBJS)

clean:
	rm -rf *.o *.a
