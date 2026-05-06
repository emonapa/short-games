CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Wno-unused -O3
LDFLAGS = -lm -ldl -lrt -lX11 -O3

SRC_DIR = src
BUILD_DIR = build

DYADICS_DIR = $(SRC_DIR)/dyadics/C
SHORTS_DIR = $(SRC_DIR)/short-games/C
SHORTS_GAMES_DIR = $(SHORTS_DIR)/games
SHORTS_INCLUDE = -I$(SHORTS_DIR)/shared

DYADICS_BUILD = $(BUILD_DIR)/dyadics
SHORTS_BUILD = $(BUILD_DIR)/short-games
SHORTS_SHARED_BUILD = $(SHORTS_BUILD)/shared

DYADICS_SRC = $(wildcard $(DYADICS_DIR)/*.c)
SHORTS_CORE_SRC = $(wildcard $(SHORTS_DIR)/*.c)

GAME_DIRS = $(wildcard $(SHORTS_GAMES_DIR)/*)
GAME_NAMES = $(notdir $(GAME_DIRS))

DYADICS_OBJ = $(patsubst $(DYADICS_DIR)/%.c,$(DYADICS_BUILD)/%.o,$(DYADICS_SRC))
DYADICS_PIC_OBJ = $(patsubst $(DYADICS_DIR)/%.c,$(DYADICS_BUILD)/%.pic.o,$(DYADICS_SRC))

SHORTS_SHARED_OBJ = $(patsubst $(SHORTS_DIR)/%.c,$(SHORTS_SHARED_BUILD)/%.o,$(SHORTS_CORE_SRC))
SHORTS_SHARED_PIC_OBJ = $(patsubst $(SHORTS_DIR)/%.c,$(SHORTS_SHARED_BUILD)/%.pic.o,$(SHORTS_CORE_SRC))

DYADICS_DEP = $(DYADICS_OBJ:.o=.d)
DYADICS_PIC_DEP = $(DYADICS_PIC_OBJ:.o=.d)

SHORTS_SHARED_DEP = $(SHORTS_SHARED_OBJ:.o=.d)
SHORTS_SHARED_PIC_DEP = $(SHORTS_SHARED_PIC_OBJ:.o=.d)

DYADICS_LIB = $(DYADICS_BUILD)/libdyadics.so

GAME_LIBS = $(foreach game,$(GAME_NAMES),$(SHORTS_BUILD)/$(game)/lib$(game).so)

.PHONY: all lib clean

all: lib

lib: $(DYADICS_LIB) $(GAME_LIBS)

$(DYADICS_LIB): $(DYADICS_PIC_OBJ)
	@mkdir -p $(@D)
	$(CC) -shared $^ -o $@ $(LDFLAGS)

$(DYADICS_BUILD)/%.o: $(DYADICS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(DYADICS_BUILD)/%.pic.o: $(DYADICS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@

$(SHORTS_SHARED_BUILD)/%.o: $(SHORTS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SHORTS_INCLUDE) -MMD -MP -c $< -o $@

$(SHORTS_SHARED_BUILD)/%.pic.o: $(SHORTS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SHORTS_INCLUDE) -fPIC -MMD -MP -c $< -o $@

define GAME_TEMPLATE

$(SHORTS_BUILD)/$(1)/lib$(1).so: \
	$$(SHORTS_SHARED_PIC_OBJ) \
	$$(patsubst $$(SHORTS_GAMES_DIR)/$(1)/%.c,$$(SHORTS_BUILD)/$(1)/%.pic.o,$$(wildcard $$(SHORTS_GAMES_DIR)/$(1)/*.c))
	@mkdir -p $$(@D)
	$$(CC) -shared $$^ -o $$@ $$(LDFLAGS)

$(SHORTS_BUILD)/$(1)/%.o: $(SHORTS_GAMES_DIR)/$(1)/%.c
	@mkdir -p $$(@D)
	$$(CC) $$(CFLAGS) $$(SHORTS_INCLUDE) -MMD -MP -c $$< -o $$@

$(SHORTS_BUILD)/$(1)/%.pic.o: $(SHORTS_GAMES_DIR)/$(1)/%.c
	@mkdir -p $$(@D)
	$$(CC) $$(CFLAGS) $$(SHORTS_INCLUDE) -fPIC -MMD -MP -c $$< -o $$@

endef

$(foreach game,$(GAME_NAMES),$(eval $(call GAME_TEMPLATE,$(game))))

clean:
	rm -rf $(BUILD_DIR)

-include $(DYADICS_DEP)
-include $(DYADICS_PIC_DEP)
-include $(SHORTS_SHARED_DEP)
-include $(SHORTS_SHARED_PIC_DEP)
-include $(wildcard $(SHORTS_BUILD)/*/*.d)
