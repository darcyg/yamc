PROJ_DIR:=.
YAMC_FILES=$(wildcard $(PROJ_DIR)/yamc/*.c)
CFLAGS:=-std=gnu11 -Og -ggdb -Wall -Wextra -Wpedantic -DYAMC_DEBUG=1 -I$(PROJ_DIR)/yamc -I$(PROJ_DIR)/wrappers
LDFLAGS:=-lrt -lpthread -lyamc -L.

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