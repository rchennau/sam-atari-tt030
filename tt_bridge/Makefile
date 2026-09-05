# ==============================================================================
# Makefile - SAM TT-Bridge HTTP Client for Atari TT030
# Targets: m68k-atari-mint-gcc / vbcc (vc)
# Atari TT030 Ecosystem Integration - Phase 1
# ==============================================================================

# Compiler Selection: Set CROSS_PREFIX or CC
CROSS_PREFIX ?= m68k-atari-mint-
CC           ?= $(CROSS_PREFIX)gcc
VBCC         ?= vc

# CPU Architecture Flags for Atari TT030 (Motorola 68030 + 68882 FPU)
# Use -m68000 for maximum compatibility across ST/STE/TT/Falcon
M68K_CPU_FLAGS ?= -m68030 -m88881

CFLAGS  ?= -O2 $(M68K_CPU_FLAGS) -Wall -Wextra -I./src
LDFLAGS ?= $(M68K_CPU_FLAGS) -s

# Source and Object definitions
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/http_client.c
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

TARGET_PRG = $(BIN_DIR)/ttbridge.prg
TARGET_VBCC = $(BIN_DIR)/ttbridge_vbcc.prg

.PHONY: all gcc vbcc clean setup install

all: setup gcc

setup:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# GCC Target (m68k-atari-mint-gcc)
gcc: setup $(TARGET_PRG)

$(TARGET_PRG): $(OBJS)
	@echo "[GCC] Linking Atari M68K PRG binary: $@"
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Build successful: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "[GCC] Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# VBCC Target (vc -sc-only -mint)
vbcc: setup
	@echo "[VBCC] Compiling with vc for Atari MiNT/TOS..."
	$(VBCC) -sc-only -mint -O2 -I./src $(SRCS) -o $(TARGET_VBCC)
	@echo "VBCC Build successful: $(TARGET_VBCC)"

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: all
	@echo "Copying $(TARGET_PRG) to staging directory..."
	@mkdir -p ../staging/AUTO
	cp $(TARGET_PRG) ../staging/AUTO/TTBRIDGE.PRG
	@echo "Installed $(TARGET_PRG) to ../staging/AUTO/TTBRIDGE.PRG"
