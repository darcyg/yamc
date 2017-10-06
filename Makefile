DEBUG_PRINT?=0
RELEASE?=0
PROJ_DIR:=.
YAMC_FILES=$(wildcard $(PROJ_DIR)/yamc/*.c)
CFLAGS:=-std=gnu11 -Wall -Wextra -Wpedantic -I$(PROJ_DIR)/yamc -I$(PROJ_DIR)/wrappers
LDFLAGS:=-lrt -lpthread -lyamc -L.

CFLAGS_DEBUG:=-Og -ggdb
CFLAGS_RELEASE:=-O3
CFLAGS_DEBUG_PRINT:=-DYAMC_DEBUG=1

ifeq ($(RELEASE),0)
	CFLAGS+=$(CFLAGS_DEBUG)
else
	CFLAGS+=$(CFLAGS_RELEASE)
endif

ifeq ($(DEBUG_PRINT),1)
	CFLAGS+=$(CFLAGS_DEBUG_PRINT)
endif

all: libyamc.a yamc_socket yamc_stdin

libyamc.a: $(YAMC_FILES:.c=.o)
	$(AR) -rcs $@ $^

yamc_socket: libyamc.a $(PROJ_DIR)/wrappers/yamc_runner_socket.o $(PROJ_DIR)/wrappers/yamc_debug_pkt_handler.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
yamc_stdin: libyamc.a $(PROJ_DIR)/wrappers/yamc_runner_stdin.o $(PROJ_DIR)/wrappers/yamc_fuzzing_pkt_handler.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f yamc_socket yamc_stdin libyamc.a $(YAMC_FILES:.c=.o) $(PROJ_DIR)/wrappers/*.o

.PHONY: all clean