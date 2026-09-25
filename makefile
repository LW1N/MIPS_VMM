CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = myvmm
OBJS = main.o utils.o

# Default target: build the executable
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target to remove compiled artifacts
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)