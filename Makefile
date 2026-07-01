CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -Wno-unused -O3
LDFLAGS = -O3

SRC_DIR = src
BUILD_DIR = build

DYADICS_DIR = $(SRC_DIR)/dyadics/C

SHORTS_DIR = $(SRC_DIR)/short-games/C
SHORTS_CORE_DIR = $(SHORTS_DIR)/core
SHORTS_CONVERT_DIR = $(SHORTS_DIR)/convert
SHORTS_CONVERT_INTERFACE_DIR = $(SHORTS_CONVERT_DIR)/convert_interface
SHORTS_GAMES_DIR = $(SHORTS_DIR)/games

DYADICS_BUILD = $(BUILD_DIR)/dyadics

SHORTS_BUILD = $(BUILD_DIR)/short-games
SHORTS_CORE_BUILD = $(SHORTS_BUILD)/core
SHORTS_CONVERT_BUILD = $(SHORTS_BUILD)/convert

DYADICS_SRC = $(wildcard $(DYADICS_DIR)/*.c)
DYADICS_PIC_OBJ = $(patsubst $(DYADICS_DIR)/%.c,$(DYADICS_BUILD)/%.pic.o,$(DYADICS_SRC))
DYADICS_PIC_DEP = $(DYADICS_PIC_OBJ:.o=.d)
DYADICS_LIB = $(DYADICS_BUILD)/libdyadics.so

SHORTS_CORE_SRC = $(wildcard $(SHORTS_CORE_DIR)/*.c)
SHORTS_CORE_PIC_OBJ = $(patsubst $(SHORTS_CORE_DIR)/%.c,$(SHORTS_CORE_BUILD)/%.pic.o,$(SHORTS_CORE_SRC))
SHORTS_CORE_PIC_DEP = $(SHORTS_CORE_PIC_OBJ:.o=.d)
SHORTS_CORE_LIB = $(SHORTS_CORE_BUILD)/libshortcore.so

SHORTS_CONVERT_SRC = $(wildcard $(SHORTS_CONVERT_DIR)/*.c)
SHORTS_CONVERT_PIC_OBJ = $(patsubst $(SHORTS_CONVERT_DIR)/%.c,$(SHORTS_CONVERT_BUILD)/%.pic.o,$(SHORTS_CONVERT_SRC))
SHORTS_CONVERT_PIC_DEP = $(SHORTS_CONVERT_PIC_OBJ:.o=.d)

GAME_DIRS = $(wildcard $(SHORTS_GAMES_DIR)/*)
GAME_NAMES = $(notdir $(GAME_DIRS))
GAME_LIBS = $(foreach game,$(GAME_NAMES),$(SHORTS_BUILD)/$(game)/lib$(game).so)

CORE_INCLUDE = -I$(SHORTS_DIR)
CONVERT_INCLUDE = -I$(SHORTS_DIR)
GAME_INCLUDE = -I$(SHORTS_DIR) -I$(SHORTS_CONVERT_INTERFACE_DIR)

.PHONY: all lib dyadics core games clean tags print

all: lib

lib: dyadics core games

dyadics: $(DYADICS_LIB)

core: $(SHORTS_CORE_LIB)

games: $(GAME_LIBS)

$(DYADICS_LIB): $(DYADICS_PIC_OBJ)
	@mkdir -p $(@D)
	$(CC) -shared $^ -o $@ $(LDFLAGS)

$(DYADICS_BUILD)/%.pic.o: $(DYADICS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@

$(SHORTS_CORE_LIB): $(SHORTS_CORE_PIC_OBJ)
	@mkdir -p $(@D)
	$(CC) -shared $^ -o $@ $(LDFLAGS)

$(SHORTS_CORE_BUILD)/%.pic.o: $(SHORTS_CORE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CORE_INCLUDE) -fPIC -MMD -MP -c $< -o $@

$(SHORTS_CONVERT_BUILD)/%.pic.o: $(SHORTS_CONVERT_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CONVERT_INCLUDE) -fPIC -MMD -MP -c $< -o $@

define GAME_TEMPLATE

$(SHORTS_BUILD)/$(1)/lib$(1).so: \
	$$(SHORTS_CORE_LIB) \
	$$(SHORTS_CONVERT_PIC_OBJ) \
	$$(patsubst $$(SHORTS_GAMES_DIR)/$(1)/%.c,$$(SHORTS_BUILD)/$(1)/%.pic.o,$$(wildcard $$(SHORTS_GAMES_DIR)/$(1)/*.c))
	@mkdir -p $$(@D)
	$$(CC) -shared \
		$$(filter %.o,$$^) \
		-o $$@ \
		-L$$(SHORTS_CORE_BUILD) \
		-Wl,-rpath,'$$$$ORIGIN/../core' \
		-lshortcore \
		$$(LDFLAGS)

$(SHORTS_BUILD)/$(1)/%.pic.o: $(SHORTS_GAMES_DIR)/$(1)/%.c
	@mkdir -p $$(@D)
	$$(CC) $$(CFLAGS) $$(GAME_INCLUDE) -fPIC -MMD -MP -c $$< -o $$@

endef

$(foreach game,$(GAME_NAMES),$(eval $(call GAME_TEMPLATE,$(game))))

tags:
	find $(SRC_DIR) -type f \( -name '*.c' -o -name '*.h' \) -print | etags -

print:
	@echo "DYADICS_SRC:"
	@printf '  %s\n' $(DYADICS_SRC)
	@echo
	@echo "SHORTS_CORE_SRC:"
	@printf '  %s\n' $(SHORTS_CORE_SRC)
	@echo
	@echo "SHORTS_CONVERT_SRC:"
	@printf '  %s\n' $(SHORTS_CONVERT_SRC)
	@echo
	@echo "GAME_NAMES:"
	@printf '  %s\n' $(GAME_NAMES)
	@echo
	@echo "GAME_LIBS:"
	@printf '  %s\n' $(GAME_LIBS)

clean:
	rm -rf $(BUILD_DIR)

-include $(DYADICS_PIC_DEP)
-include $(SHORTS_CORE_PIC_DEP)
-include $(SHORTS_CONVERT_PIC_DEP)
-include $(wildcard $(SHORTS_BUILD)/*/*.d)
