# gchttp Makefile

DISPATCHER_DIR := dispatcher
HTTP_DIR := http
TEST_DIR := test
CLIENT_DIR := client

SOURCES := $(wildcard *.c) \
	$(wildcard $(DISPATCHER_DIR)/*.c) \
	$(wildcard $(HTTP_DIR)/*.c)	\
	$(wildcard $(TEST_DIR)/*.c)

OBJS := $(patsubst %.c,%.o,$(SOURCES))

# DIRS := $(shell find . -maxdepth 3 -type d)
# SOURCES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.c))

SERVER_SOURCES := $(wildcard *.c) \
	$(wildcard $(DISPATCHER_DIR)/*.c)
SERVER_OBJS := $(patsubst %.c,%.o,$(SERVER_SOURCES))

HTTP_SOURCES := $(wildcard *.c) \
	$(wildcard $(DISPATCHER_DIR)/*.c) \
	$(wildcard $(HTTP_DIR)/*.c)
HTTP_OBJS := $(patsubst %.c,%.o,%(HTTP_SOURCES))

# TARGET := $(notdir $(CURDIR))
TARGET := SERVER

# compiler, linker, lib, paramters, etc.
CC := gcc
LD := gcc
DEFINES := -DEPOLL_ENABLED
INCLUDE := -I.
CFLAGS := -g -Wall -O0 $(DEFINES) $(INCLUDE)
LDFLAGS :=
LIBS := -lpthread

.PHONY: all cgdb-tcpserver source clean

all: $(TARGET) gc_tcpclient
	@echo "done!"

$(TARGET): clean $($(addsuffix _OBJS, $(TARGET))) 
	@echo "linking objects to gc_tcpserver ..."
	$(LD) $(LDFALGS) $($(addsuffix _OBJS, $(TARGET))) $(LIBS) -o gc_tcpserver 
	@echo "build successfully!"

acceptor.o: common.h
	@echo "compiling acceptor ..."
	$(CC) $(CFLAGS) -c acceptor.c
	
channel.o:
	@echo "compiling channel ..."
	$(CC) $(CFLAGS) -c channel.c

channel_map.o:
	@echo "compiling channel_map ..."
	$(CC) $(CFLAGS) -c channel_map.c

event_loop.o: channel.h channel_map.h common.h event_dispatcher.h
	@echo "compiling event_loop ..."
	$(CC) $(CFLAGS) -c event_loop.c

select_dispatcher.o:
	@echo "compiling select_dispatcher ..."
	$(CC) $(CFLAGS) -c dispathcer/select_dispatcher.c

poll_dispatcher.o:
	@echo "compiling poll_dispatcher ..."
	$(CC) $(CFLAGS) -c dispathcer/poll_dispatcher.c

epoll_dispatcher.o:
	@echo "compiling epoll_dispatcher ..."
	$(CC) $(CFLAGS) -c dispathcer/epoll_dispatcher.c

thread_pool.o: event_loop_thread.h
	@echo "compiling thread_pool ..."
	$(CC) $(CFLAGS) -c thread_pool.c

event_loop_thread.o: log.h
	@echo "compiling event_loop_thread ..."
	$(CC) $(CFLAGS) -c event_loop_thread.c

buffer.o: log.h
	@echo "compiling buffer ..."
	$(CC) $(CFLAGS) -c buffer.c

tcp_connection.o: channel.h event_loop.h
	@echo "compiling tcp_connection ..."
	$(CC) $(CFLAGS) -c tcp_connection.c

server.o: acceptor.h channel.h event_loop.h buffer.h tcp_connection.h thread_pool.h common.h
	@echo "compiling server ..."
	$(CC) $(CFLAGS) -c server.c

common.o: log.h
	@echo "compiling common ..."
	$(CC) $(CFLAGS) -c common.c

log.o:
	@echo "compiling log ..."
	$(CC) $(CFLAGS) -c log.c

gc_tcpclient:
	cd $(CLIENT_DIR);$(CC) $(CFLAGS) gc_tcpclient.c -o gc_tcpclient;cd ..

cgdb-tcpserver:
	cgdb gc_tcpserver

source:
	@echo "[ALL] $(SOURCES)"
	@echo "[SERVER] $(SERVER_SOURCES)"
	@echo "[HTTP] $(HTTP_SOURCES)"

clean:
	@echo "cleaning all object file..."
	-rm -f *.o
	cd $(CLIENT_DIR);rm gc_tcpclient;cd ..
