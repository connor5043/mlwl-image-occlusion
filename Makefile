# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_XOPEN_SOURCE=500
LIBS = -lSDL2 -lSDL2_image -lcurl

# Files
SRC = mlwl-image-occlusion.c
OUT = mlwl-image-occlusion

# Script
INSTALL_BIN_DIR = $(HOME)/.local/bin
INSTALL_SHARE_DIR = $(HOME)/.local/share/mlwl-image-occlusion
SCRIPT = ./script.sh

# Default target
all: $(OUT) script

# Compile the main program
$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

# Install the program and script
install: $(OUT)
	@echo "Installing the program to $(INSTALL_BIN_DIR)..."
	mkdir -p $(INSTALL_BIN_DIR)
	cp $(OUT) $(INSTALL_BIN_DIR)/
	chmod +x $(INSTALL_BIN_DIR)/$(OUT)

	@echo "Installing the script to $(INSTALL_SHARE_DIR)..."
	mkdir -p $(INSTALL_SHARE_DIR)
	cp $(SCRIPT) $(INSTALL_SHARE_DIR)/
	chmod +x $(INSTALL_SHARE_DIR)/$(SCRIPT)
	@echo "Installation complete."

# Ensure the script is executable
script:
	chmod +x $(SCRIPT)

# Clean up build artifacts
clean:
	rm -f $(OUT) $(DEBUG_OUT)
