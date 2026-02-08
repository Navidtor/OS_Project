# ALFS - Anushiravan-Level Fair Scheduler
# Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2
CFLAGS += -I./include -I./lib
DEBUG_FLAGS = -g -DDEBUG -O0 -fsanitize=address -fsanitize=undefined

# Source files
SRC_DIR = src
LIB_DIR = lib
INCLUDE_DIR = include
TEST_DIR = tests

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/heap.c \
       $(SRC_DIR)/task.c \
       $(SRC_DIR)/cgroup.c \
       $(SRC_DIR)/scheduler.c \
       $(SRC_DIR)/uds.c \
       $(SRC_DIR)/json_handler.c \
       $(LIB_DIR)/cJSON/cJSON.c

OBJS = $(SRCS:.c=.o)
TARGET = alfs_scheduler

# Library objects (without main)
LIB_SRCS = $(SRC_DIR)/heap.c \
           $(SRC_DIR)/task.c \
           $(SRC_DIR)/cgroup.c \
           $(SRC_DIR)/scheduler.c \
           $(SRC_DIR)/uds.c \
           $(SRC_DIR)/json_handler.c \
           $(LIB_DIR)/cJSON/cJSON.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

# Test executables
TEST_HEAP_BIN = test_heap_runner
TEST_SCHED_BIN = test_scheduler_runner

.PHONY: all clean debug test test_heap test_scheduler install dist help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Test targets
test: test_heap test_scheduler

test_heap: $(TEST_HEAP_BIN)
	./$(TEST_HEAP_BIN)

test_scheduler: $(TEST_SCHED_BIN)
	./$(TEST_SCHED_BIN)

$(TEST_HEAP_BIN): $(TEST_DIR)/test_heap.c $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_SCHED_BIN): $(TEST_DIR)/test_scheduler.c $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Clean
clean:
	rm -f $(OBJS) $(TARGET) $(TEST_HEAP_BIN) $(TEST_SCHED_BIN)
	rm -f $(SRC_DIR)/*.o $(LIB_DIR)/cJSON/*.o $(TEST_DIR)/*.o

# Install (copy to /usr/local/bin)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Create distribution archive
dist: clean
	tar -czvf alfs_scheduler.tar.gz \
		Makefile README.md IMPLEMENTATION_PLAN.md \
		$(INCLUDE_DIR)/*.h $(SRC_DIR)/*.c $(LIB_DIR)/cJSON/*.c $(LIB_DIR)/cJSON/*.h \
		$(TEST_DIR)/*.c $(TEST_DIR)/*.json docs/

# Show help
help:
	@echo "ALFS Scheduler Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all            - Build the scheduler (default)"
	@echo "  debug          - Build with debug flags and sanitizers"
	@echo "  test           - Build and run all tests"
	@echo "  test_heap      - Build and run heap tests only"
	@echo "  test_scheduler - Build and run scheduler tests only"
	@echo "  clean          - Remove build artifacts"
	@echo "  install        - Install to /usr/local/bin"
	@echo "  dist           - Create distribution archive"
	@echo "  help           - Show this help message"
