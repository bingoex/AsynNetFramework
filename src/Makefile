

DEBUG= -g

CSRC=$(shell ls *.c)
CPPSRC=$(shell ls *.cpp)

OBJS=$(CSRC:.c=.o) 
#OBJS+= $(SPPSRC:.cpp=.o)

COMM_DIR = ./comm/src/

#-lr -lstdc++
COMM_LIB = $(COMM_DIR)/cm_lib.a -I$(COMM_DIR) 

ARGS = -g -Wall -fno-strict-aliasing

CC = g++


.c.o:
	$(CC) $(ARGS) -c $*.c $(COMM_LIB)

all: $(OBJS)
	rm -f amf.a
	ar -q amf.a $(OBJS)

print:
	@echo $(CSRC)
	@echo $(CPPSRC)
	@echo $(OBJS)

clean:
	rm -rf *.o *.a
