CXX = gcc
CFLAGS = -std=c99 -w

TARGET = filesys
EXE = filesys

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CXX) $(CFLAGS) -o $(EXE).x $(TARGET).c