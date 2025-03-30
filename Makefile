C=gcc
CFLAGS=-Wall -Werror
INCLUDE=-lrt -lm -lasound -ljack -lpulse -lpulse-simple -pthread -lopus -logg -lvorbisenc -lvorbis -lmpg123 -lid3tag -lmp3lame -lncursesw

all: editor

editor: src/main.c
	$(C) src/*.c libportaudio.a $(CFLAGS) $(INCLUDE) $(shell pkg-config --cflags --libs flac sndfile) -o player

clean:
	rm -rf *.o player

install: editor
	sudo cp editor /bin/
