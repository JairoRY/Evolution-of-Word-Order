# Compiler and flags
CXX := g++
CXXFLAGS := -O3 -std=c++20 -Wall -D_GLIBCXX_DEBUG

# Source files
SRC_PCEEC := processPCEEC.cpp
SRC_SUSANNE := processSusanne.cpp

# Executable names
BIN_PCEEC := processPCEEC
BIN_SUSANNE := processSusanne

# Default target builds both
all: $(BIN_PCEEC) $(BIN_SUSANNE)

# Build PCEEC binary
$(BIN_PCEEC): $(SRC_PCEEC)
	$(CXX) $(CXXFLAGS) $< -o $@

# Build Susanne binary
$(BIN_SUSANNE): $(SRC_SUSANNE)
	$(CXX) $(CXXFLAGS) $< -o $@

# Run PCEEC
run_pceec: $(BIN_PCEEC)
	./$(BIN_PCEEC)

# Run Susanne
run_susanne: $(BIN_SUSANNE)
	./$(BIN_SUSANNE)

# Clean up
clean:
	rm -f $(BIN_PCEEC) $(BIN_SUSANNE)

.PHONY: all run_pceec run_susanne clean

