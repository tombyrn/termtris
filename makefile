CC = clang
CFLAGS  = -g -Wall -Werror -std=c17

TARGET = tetris

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(TARGET).c $(CFLAGS) -lpthread -o $(TARGET)

clean:
	$(RM) -rf $(TARGET) $(TARGET).dSYM