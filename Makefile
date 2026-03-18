CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra

ifdef DEBUG
    CXXFLAGS += -g -O0
else
    CXXFLAGS += -O2
endif

SRC_DIR := src
OBJ_DIR := obj

SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
OBJS := $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(SRCS))

TARGET := bare-compiler

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)