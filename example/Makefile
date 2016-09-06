
DEBUG=-g

CSRC=$(shell ls *.c)
CPPSRC=$(shell ls *.cpp)

OBJS=$(CSRC:.c=.o) 

COMM_DIR = ../comm/src

#-lr -lstdc++
COMM_LIB = $(COMM_DIR)/cm_lib.a -I$(COMM_DIR) 

ANF_DIR = ../src

ANF_LIB = $(ANF_DIR)/anf.a -I$(ANF_DIR)

ALL_BINARY = server test_client

ARGS = -g -Wall -fno-strict-aliasing #-Qunused-arguments

CC = g++

.c.o:
	$(CC) -c $*.c $(COMM_LIB) $(ANF_LIB)

ALL : $(ALL_BINARY)

#$(ALL_BINARY) : $(OBJS)
#	$(CC) $(ARGS) -o $@ $^ $(COMM_LIB)

server : server.o
	$(CC) $(ARGS) -o $@ $^ $(COMM_LIB) $(ANF_LIB)
	
test_client : test_client.o
	$(CC) $(ARGS) -o $@ $^ $(COMM_LIB) $(ANF_LIB)


clean:
	rm -rf *.o *.a $(ALL_BINARY)
