# ------------------------------------------------------------
# Build mode
# ------------------------------------------------------------
DEBUG ?= 0

ifeq ($(DEBUG),1)
    BUILD_DIR := build/debug
    TEST_BUILD_DIR := tests/build/debug
    CFLAGS := -Wall -Wextra -Werror -g -O0 -fPIC -DDEBUG
else
    BUILD_DIR := build/release
    TEST_BUILD_DIR := tests/build/release
    CFLAGS := -Wall -Wextra -Werror -O2 -fPIC
endif

CC := gcc
AR := ar rcs
INCLUDES := -I. -Iinclude -Itests

# ------------------------------------------------------------
# Optional libunistring
# ------------------------------------------------------------
ENABLE_UNISTRING ?= 1

ifeq ($(ENABLE_UNISTRING),1)
    UNISTRING_CFLAGS := $(shell pkg-config --cflags libunistring 2>/dev/null)
    UNISTRING_LIBS   := $(shell pkg-config --libs libunistring 2>/dev/null)

    ifneq ($(UNISTRING_CFLAGS)$(UNISTRING_LIBS),)
        CFLAGS += $(UNISTRING_CFLAGS) -DHAVE_UNISTRING
        LDLIBS += $(UNISTRING_LIBS)
    else
        CFLAGS += -DHAVE_UNISTRING
        LDLIBS += -lunistring
    endif
endif

LDLIBS += -lm

# ------------------------------------------------------------
# Source discovery
# ------------------------------------------------------------
SRCS := $(shell find src -name '*.c')
OBJS := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

TEST_SRCS := $(wildcard tests/test_*.c)
TEST_OBJS := $(TEST_SRCS:tests/%.c=$(TEST_BUILD_DIR)/%.o)

HEADERS := $(wildcard include/*.h)
TEST_HEADERS := tests/test_string.h

STATIC_LIB := $(BUILD_DIR)/libooc.a
SHARED_LIB := $(BUILD_DIR)/libooc.so

TEST_NAMES := $(patsubst tests/test_%.c,%,$(TEST_SRCS))
TEST_BINS := $(TEST_NAMES:%=$(TEST_BUILD_DIR)/test_%)

# ------------------------------------------------------------
# Rules
# ------------------------------------------------------------
.PHONY: all clean test memtest debug release

all: $(STATIC_LIB) $(SHARED_LIB) $(TEST_BINS)

debug:
	$(MAKE) DEBUG=1 all

release:
	$(MAKE) DEBUG=0 all

$(OBJS): $(HEADERS)
$(TEST_OBJS): $(HEADERS) $(TEST_HEADERS)

# ------------------------------------------------------------
# Object build rules
# ------------------------------------------------------------
$(BUILD_DIR)/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: tests/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ------------------------------------------------------------
# Libraries
# ------------------------------------------------------------
$(STATIC_LIB): $(OBJS)
	mkdir -p $(dir $@)
	$(AR) $@ $(OBJS)

$(SHARED_LIB): $(OBJS)
	mkdir -p $(dir $@)
	$(CC) -shared -o $@ $(OBJS) $(LDLIBS)

# ------------------------------------------------------------
# Test binaries (with correct rpath)
# ------------------------------------------------------------
$(TEST_BUILD_DIR)/test_%: $(TEST_BUILD_DIR)/test_%.o $(STATIC_LIB)
	mkdir -p $(dir $@)
	$(CC) -o $@ $< \
    	-L$(BUILD_DIR) -looc \
    	-Wl,-rpath,'$$ORIGIN:$$ORIGIN/../../../$(BUILD_DIR)' \
    	$(LDLIBS)

# ------------------------------------------------------------
# Test targets
# ------------------------------------------------------------
test: $(TEST_BINS)
	for t in $(TEST_BINS); do $$t; done

memtest: $(TEST_BINS)
	for t in $(TEST_BINS); do valgrind --leak-check=full --show-leak-kinds=all $$t; done

test_%: $(TEST_BUILD_DIR)/test_%
	$<

memtest_%: $(TEST_BUILD_DIR)/test_%
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $<

# VALGRIND = valgrind \
# 	--leak-check=full \
# 	--show-leak-kinds=all \
# 	--track-origins=yes \
# 	--verbose \
# 	--log-file=valgrind_$*.log

# memtest_%: $(TEST_BUILD_DIR)/test_%
# 	$(VALGRIND) $<
# 	@echo "----------------------------------------"
# 	@echo "Valgrind log saved to: valgrind_$*.log"
# 	@echo "Search for leaked pointers with:"
# 	@echo "    grep -R \"LEAK SUMMARY\" -n valgrind_$*.log"

# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------
clean:
	rm -rf build tests/build
