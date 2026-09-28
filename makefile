CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
TARGET = myvmm
OBJS = myvmm.o

.PHONY: all clean debug sanitize test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

debug: CFLAGS += -g -O0
debug: clean all

sanitize: CFLAGS += -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
sanitize: clean all

test: all
	./tests/run_tests.sh
