CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Wno-unused -O3 
LDFLAGS = -lm -lpthread -ldl -lrt -lX11 -O3

SRC_DIR = src
BUILD_DIR = build

RB_DIR = $(SRC_DIR)/RB
RBG_DIR = $(SRC_DIR)/RBG

RB_BUILD = $(BUILD_DIR)/RB
RBG_BUILD = $(BUILD_DIR)/RBG

# Sources
RB_SRC = $(wildcard $(RB_DIR)/*.c)
RBG_SRC = $(wildcard $(RBG_DIR)/*.c)

# Normal objects + deps
RB_OBJ = $(patsubst $(RB_DIR)/%.c,$(RB_BUILD)/%.o,$(RB_SRC))
RBG_OBJ = $(patsubst $(RBG_DIR)/%.c,$(RBG_BUILD)/%.o,$(RBG_SRC))

RB_DEP = $(RB_OBJ:.o=.d)
RBG_DEP = $(RBG_OBJ:.o=.d)

# PIC objects + deps (shared lib)
RB_PIC_OBJ = $(patsubst $(RB_DIR)/%.c,$(RB_BUILD)/%.pic.o,$(RB_SRC))
RBG_PIC_OBJ = $(patsubst $(RBG_DIR)/%.c,$(RBG_BUILD)/%.pic.o,$(RBG_SRC))

RB_PIC_DEP = $(RB_PIC_OBJ:.o=.d)
RBG_PIC_DEP = $(RBG_PIC_OBJ:.o=.d)

# Outputs
RB_BIN = RB
RBG_BIN = RBG

RB_LIB = $(RB_BUILD)/libhb.so
RBG_LIB = $(RBG_BUILD)/libhb.so

.PHONY: all clean run run_rb run_rbg lib lib_rb lib_rbg

# Build everything
all: $(RB_BIN) $(RBG_BIN) lib

# Link binaries
$(RB_BIN): $(RB_OBJ)
	$(CC) $(RB_OBJ) -o $@ $(LDFLAGS)

$(RBG_BIN): $(RBG_OBJ)
	$(CC) $(RBG_OBJ) -o $@ $(LDFLAGS)

# Shared libs
lib: $(RB_LIB) $(RBG_LIB)
lib_rb: $(RB_LIB)
lib_rbg: $(RBG_LIB)

$(RB_LIB): $(RB_PIC_OBJ) | $(RB_BUILD)
	$(CC) -shared $(RB_PIC_OBJ) -o $@ $(LDFLAGS)

$(RBG_LIB): $(RBG_PIC_OBJ) | $(RBG_BUILD)
	$(CC) -shared $(RBG_PIC_OBJ) -o $@ $(LDFLAGS)

# Compile normal objects
$(RB_BUILD)/%.o: $(RB_DIR)/%.c | $(RB_BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(RBG_BUILD)/%.o: $(RBG_DIR)/%.c | $(RBG_BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Compile PIC objects
$(RB_BUILD)/%.pic.o: $(RB_DIR)/%.c | $(RB_BUILD)
	$(CC) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@

$(RBG_BUILD)/%.pic.o: $(RBG_DIR)/%.c | $(RBG_BUILD)
	$(CC) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@

# Build dirs
$(RB_BUILD):
	mkdir -p $(RB_BUILD)

$(RBG_BUILD):
	mkdir -p $(RBG_BUILD)

# Run helpers
run: $(RB_BIN) $(RBG_BIN)
	./$(RB_BIN)
	./$(RBG_BIN)

run_rb: $(RB_BIN)
	./$(RB_BIN)

run_rbg: $(RBG_BIN)
	./$(RBG_BIN)

clean:
	rm -rf $(BUILD_DIR) $(RB_BIN) $(RBG_BIN)

-include $(RB_DEP)
-include $(RBG_DEP)
-include $(RB_PIC_DEP)
-include $(RBG_PIC_DEP)
