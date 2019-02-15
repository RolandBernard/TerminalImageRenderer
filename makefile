SRC=./src
BUILD=./build
LIB=./lib/bin
INCLUDE=./lib/include

LIBS=
ARGS=-g -Wall
OBJECTS=$(BUILD)/tir.o
TARGET=./test
LIBTARGET=libtir.a
CC=gcc
CLEAN=rm -f
COPY=cp -R

all: ./lib $(TARGET)

./lib: $(OBJECTS)
	$(COPY) $(SRC)/tir.h $(INCLUDE)/
	ar rcs $(LIB)/$(LIBTARGET) $(OBJECTS)

$(TARGET): $(OBJECTS) $(BUILD)/test.o
	$(CC) $(ARGS) -o $(TARGET) $(OBJECTS) $(BUILD)/test.o $(LIBS)

$(BUILD)/test.o: $(SRC)/test.c $(SRC)/tir.h
	$(CC) $(ARGS) -c -o $(BUILD)/test.o $(SRC)/test.c $(LIBS)

$(BUILD)/tir.o: $(SRC)/tir.c $(SRC)/tir.h
	$(CC) $(ARGS) -c -o $(BUILD)/tir.o $(SRC)/tir.c $(LIBS)

clean:
	$(CLEAN) $(OBJECTS) $(BUILD)/test.o

cleanall:
	$(CLEAN) $(OBJECTS) $(BUILD)/test.o $(TARGET) $(LIBTARGET)
