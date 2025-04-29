C=gcc
CFLAGS=-Wall -Werror
INCLUDE=-lportaudio -lrt -lm -lasound -ljack -lpulse -lpulse-simple -pthread -lopus -logg -lvorbisenc -lvorbis -lmpg123 -lid3tag -lmp3lame -lncursesw

all: player

player: src/main.c
	$(C) src/*.c $(CFLAGS) $(INCLUDE) $(shell pkg-config --cflags --libs flac sndfile) -o player

clean:
	rm -rf *.o player

install: player
	sudo cp player /bin/
