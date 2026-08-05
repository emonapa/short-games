CC ?= gcc

CFLAGS ?= -Wall -Wextra -std=c11 -Wno-unused -O3
CPPFLAGS ?=
LDFLAGS ?= -O3
LDLIBS ?= -lm

SRC_DIR := src
BUILD_DIR := build

# -----------------------------------------------------------------------------
# Dyadics
# -----------------------------------------------------------------------------

DYADICS_DIR := $(SRC_DIR)/dyadics/C
DYADICS_BUILD := $(BUILD_DIR)/dyadics

DYADICS_SRC := $(wildcard $(DYADICS_DIR)/*.c)
DYADICS_PIC_OBJ := \
	$(patsubst $(DYADICS_DIR)/%.c,$(DYADICS_BUILD)/%.pic.o,$(DYADICS_SRC))
DYADICS_PIC_DEP := $(DYADICS_PIC_OBJ:.o=.d)
DYADICS_LIB := $(DYADICS_BUILD)/libdyadics.so

DYADICS_INCLUDE := -I$(DYADICS_DIR)

# -----------------------------------------------------------------------------
# Short games
# -----------------------------------------------------------------------------

SHORTS_DIR := $(SRC_DIR)/short-games/C
SHORTS_CORE_DIR := $(SHORTS_DIR)/core
SHORTS_PARSER_DIR := $(SHORTS_CORE_DIR)/parser
SHORTS_SHARED_DIR := $(SHORTS_DIR)/shared
SHORTS_CONVERT_DIR := $(SHORTS_DIR)/convert
SHORTS_CONVERT_INTERFACE_DIR := $(SHORTS_CONVERT_DIR)/convert_interface
SHORTS_GAMES_DIR := $(SHORTS_DIR)/games

SHORTS_BUILD := $(BUILD_DIR)/short-games
SHORTS_CORE_BUILD := $(SHORTS_BUILD)/core
SHORTS_PARSER_BUILD := $(SHORTS_CORE_BUILD)/parser
SHORTS_CONVERT_BUILD := $(SHORTS_BUILD)/convert

SHORTS_CORE_LIB := $(SHORTS_CORE_BUILD)/libshortcore.so

# core/game_string.c is the old implementation.
# The new public game_from_string() implementation is parser/game_string.c.
SHORTS_CORE_SRC := $(filter-out \
	$(SHORTS_CORE_DIR)/game_string.c, \
	$(wildcard $(SHORTS_CORE_DIR)/*.c))

# Only production parser sources are included.
# example_ast.c, example_evaluation.c and test_parser.c are intentionally omitted.
SHORTS_PARSER_SRC := \
	$(SHORTS_PARSER_DIR)/ast.c \
	$(SHORTS_PARSER_DIR)/dyadic_value.c \
	$(SHORTS_PARSER_DIR)/game_format.c \
	$(SHORTS_PARSER_DIR)/game_string.c \
	$(SHORTS_PARSER_DIR)/language_error.c \
	$(SHORTS_PARSER_DIR)/lexer.c \
	$(SHORTS_PARSER_DIR)/parser.c \
	$(SHORTS_PARSER_DIR)/semantic.c \
	$(SHORTS_PARSER_DIR)/token.c

SHORTS_CORE_PIC_OBJ := \
	$(patsubst $(SHORTS_CORE_DIR)/%.c,$(SHORTS_CORE_BUILD)/%.pic.o,$(SHORTS_CORE_SRC))

SHORTS_PARSER_PIC_OBJ := \
	$(patsubst $(SHORTS_PARSER_DIR)/%.c,$(SHORTS_PARSER_BUILD)/%.pic.o,$(SHORTS_PARSER_SRC))

SHORTS_ALL_PIC_OBJ := $(SHORTS_CORE_PIC_OBJ) $(SHORTS_PARSER_PIC_OBJ)
SHORTS_ALL_PIC_DEP := $(SHORTS_ALL_PIC_OBJ:.o=.d)

CORE_INCLUDE := \
	-I$(SHORTS_DIR) \
	-I$(SHORTS_CORE_DIR) \
	-I$(SHORTS_PARSER_DIR) \
	-I$(SHORTS_SHARED_DIR)

# -----------------------------------------------------------------------------
# Conversion layer
# -----------------------------------------------------------------------------

SHORTS_CONVERT_SRC := $(wildcard $(SHORTS_CONVERT_DIR)/*.c)
SHORTS_CONVERT_PIC_OBJ := \
	$(patsubst $(SHORTS_CONVERT_DIR)/%.c,$(SHORTS_CONVERT_BUILD)/%.pic.o,$(SHORTS_CONVERT_SRC))
SHORTS_CONVERT_PIC_DEP := $(SHORTS_CONVERT_PIC_OBJ:.o=.d)

CONVERT_INCLUDE := \
	-I$(SHORTS_DIR) \
	-I$(SHORTS_CORE_DIR) \
	-I$(SHORTS_PARSER_DIR) \
	-I$(SHORTS_SHARED_DIR) \
	-I$(SHORTS_CONVERT_DIR) \
	-I$(SHORTS_CONVERT_INTERFACE_DIR)

# -----------------------------------------------------------------------------
# Individual games
# -----------------------------------------------------------------------------

GAME_DIRS := $(wildcard $(SHORTS_GAMES_DIR)/*)
GAME_NAMES := $(notdir $(GAME_DIRS))
GAME_LIBS := $(foreach game,$(GAME_NAMES),$(SHORTS_BUILD)/$(game)/lib$(game).so)

GAME_INCLUDE := \
	-I$(SHORTS_DIR) \
	-I$(SHORTS_CORE_DIR) \
	-I$(SHORTS_PARSER_DIR) \
	-I$(SHORTS_SHARED_DIR) \
	-I$(SHORTS_CONVERT_DIR) \
	-I$(SHORTS_CONVERT_INTERFACE_DIR)

# -----------------------------------------------------------------------------
# Python package
# -----------------------------------------------------------------------------

PYTHON_SOURCE_DIR := $(SRC_DIR)/short-games/python
PYTHON_PACKAGE_NAME := shortg
PYTHON_PACKAGE_TEMPLATE_DIR := packaging/$(PYTHON_PACKAGE_NAME)
PYTHON_PACKAGE_DIR := $(PYTHON_PACKAGE_NAME)
PYTHON_PACKAGE_MODULE_DIR := \
	$(PYTHON_PACKAGE_DIR)/src/$(PYTHON_PACKAGE_NAME)

# -----------------------------------------------------------------------------
# Public targets
# -----------------------------------------------------------------------------

.PHONY: all lib dyadics core games python-lib parser-test parser-example clean tags print

all: lib

lib: dyadics core games

dyadics: $(DYADICS_LIB)

core: $(SHORTS_CORE_LIB)

games: $(GAME_LIBS)

python-lib: $(SHORTS_CORE_LIB)
	@mkdir -p $(PYTHON_PACKAGE_MODULE_DIR)
	cp $(PYTHON_PACKAGE_TEMPLATE_DIR)/pyproject.toml $(PYTHON_PACKAGE_DIR)/
	cp $(PYTHON_PACKAGE_TEMPLATE_DIR)/README.md $(PYTHON_PACKAGE_DIR)/
	cp $(PYTHON_PACKAGE_TEMPLATE_DIR)/__init__.py $(PYTHON_PACKAGE_MODULE_DIR)/
	cp $(PYTHON_SOURCE_DIR)/game.py $(PYTHON_PACKAGE_MODULE_DIR)/
	cp $(PYTHON_SOURCE_DIR)/game_runtime.py $(PYTHON_PACKAGE_MODULE_DIR)/
	cp $(SHORTS_CORE_LIB) $(PYTHON_PACKAGE_MODULE_DIR)/

# -----------------------------------------------------------------------------
# Dyadics shared library
# -----------------------------------------------------------------------------

$(DYADICS_LIB): $(DYADICS_PIC_OBJ)
	@mkdir -p $(@D)
	$(CC) -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(DYADICS_BUILD)/%.pic.o: $(DYADICS_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DYADICS_INCLUDE) \
		-fPIC -MMD -MP -c $< -o $@

# -----------------------------------------------------------------------------
# Short-games core shared library
# -----------------------------------------------------------------------------

$(SHORTS_CORE_LIB): $(SHORTS_ALL_PIC_OBJ)
	@mkdir -p $(@D)
	$(CC) -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(SHORTS_CORE_BUILD)/%.pic.o: $(SHORTS_CORE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_INCLUDE) \
		-fPIC -MMD -MP -c $< -o $@

$(SHORTS_PARSER_BUILD)/%.pic.o: $(SHORTS_PARSER_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_INCLUDE) \
		-fPIC -MMD -MP -c $< -o $@

# -----------------------------------------------------------------------------
# Conversion objects
# -----------------------------------------------------------------------------

$(SHORTS_CONVERT_BUILD)/%.pic.o: $(SHORTS_CONVERT_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CONVERT_INCLUDE) \
		-fPIC -MMD -MP -c $< -o $@

# -----------------------------------------------------------------------------
# Per-game shared libraries
# -----------------------------------------------------------------------------

define GAME_TEMPLATE

GAME_$(1)_SRC := $$(wildcard $$(SHORTS_GAMES_DIR)/$(1)/*.c)
GAME_$(1)_OBJ := $$(patsubst \
	$$(SHORTS_GAMES_DIR)/$(1)/%.c, \
	$$(SHORTS_BUILD)/$(1)/%.pic.o, \
	$$(GAME_$(1)_SRC))

$$(SHORTS_BUILD)/$(1)/lib$(1).so: \
	$$(SHORTS_CORE_LIB) \
	$$(SHORTS_CONVERT_PIC_OBJ) \
	$$(GAME_$(1)_OBJ)
	@mkdir -p $$(@D)
	$$(CC) -shared \
		$$(SHORTS_CONVERT_PIC_OBJ) \
		$$(GAME_$(1)_OBJ) \
		-o $$@ \
		-L$$(SHORTS_CORE_BUILD) \
		-Wl,-rpath,'$$$$ORIGIN/../core' \
		-lshortcore \
		$$(LDFLAGS) \
		$$(LDLIBS)

$$(SHORTS_BUILD)/$(1)/%.pic.o: $$(SHORTS_GAMES_DIR)/$(1)/%.c
	@mkdir -p $$(@D)
	$$(CC) $$(CPPFLAGS) $$(CFLAGS) $$(GAME_INCLUDE) \
		-fPIC -MMD -MP -c $$< -o $$@

endef

$(foreach game,$(GAME_NAMES),$(eval $(call GAME_TEMPLATE,$(game))))

# -----------------------------------------------------------------------------
# Optional parser executables
# -----------------------------------------------------------------------------

PARSER_TEST := $(SHORTS_PARSER_BUILD)/test_parser
PARSER_EXAMPLE := $(SHORTS_PARSER_BUILD)/example_evaluation

parser-test: $(PARSER_TEST)

$(PARSER_TEST): $(SHORTS_PARSER_DIR)/test_parser.c $(SHORTS_CORE_LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_INCLUDE) \
		$< -o $@ \
		-L$(SHORTS_CORE_BUILD) \
		-Wl,-rpath,'$$ORIGIN/..' \
		-lshortcore \
		$(LDFLAGS) \
		$(LDLIBS)

parser-example: $(PARSER_EXAMPLE)

$(PARSER_EXAMPLE): $(SHORTS_PARSER_DIR)/example_evaluation.c $(SHORTS_CORE_LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_INCLUDE) \
		$< -o $@ \
		-L$(SHORTS_CORE_BUILD) \
		-Wl,-rpath,'$$ORIGIN/..' \
		-lshortcore \
		$(LDFLAGS) \
		$(LDLIBS)

# -----------------------------------------------------------------------------
# Utilities
# -----------------------------------------------------------------------------

tags:
	find $(SRC_DIR) -type f \( -name '*.c' -o -name '*.h' \) -print | etags -

print:
	@echo "DYADICS_SRC:"
	@printf '  %s\n' $(DYADICS_SRC)
	@echo
	@echo "SHORTS_CORE_SRC:"
	@printf '  %s\n' $(SHORTS_CORE_SRC)
	@echo
	@echo "SHORTS_PARSER_SRC:"
	@printf '  %s\n' $(SHORTS_PARSER_SRC)
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
	rm -rf $(BUILD_DIR) $(PYTHON_PACKAGE_DIR)

-include $(DYADICS_PIC_DEP)
-include $(SHORTS_ALL_PIC_DEP)
-include $(SHORTS_CONVERT_PIC_DEP)
-include $(wildcard $(SHORTS_BUILD)/*/*.d)
