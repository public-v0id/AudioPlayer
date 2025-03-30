#include <stdio.h>
#include <string.h>
#include <portaudio.h>
#include <math.h>
#include <ncurses.h>
#include <sndfile.h>
#include <stdlib.h>
#include <complex.h>
#include <pthread.h>
#include <stdbool.h>

#define TABLE_SIZE 200
#define PI (3.14159265)
#define CHANNELS 2
typedef struct {
	SNDFILE* file;
	SF_INFO sfinfo;
	float* buffer;
	long framesRemaining;
} audioData;

size_t acc, yres;
volatile bool paused;

static int outputCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
				void *userData) {
	float* out = (float*)outputBuffer;
	long framesToWrite = framesPerBuffer;
	audioData* data = (audioData*) userData;
	long framesRead;
	if (data->framesRemaining == 0) {
		return paComplete;
	}
	if (data->framesRemaining < framesPerBuffer) {
		framesToWrite = data->framesRemaining;
	}
	framesRead = sf_readf_float(data->file, out, framesToWrite);
	memcpy(data->buffer, out, 8*framesToWrite);
	if (framesRead < framesToWrite) {
		return paComplete;
	}
	return paContinue;
}

typedef struct {
	unsigned long framesPerBuffer;
	void* userData;
} graphArgs;

void* graphOut(void *args) {
	graphArgs *grargs = (graphArgs*)args;
	while (true) {
		if (paused) {
			continue;
		}
		float *rptr = ((audioData*)(grargs->userData))->buffer;	
		clear();
		for (size_t k = 0; k < acc/2; ++k) {
			float complex im = CMPLX(0,1);
			float complex x = 0;	
			for (int i = 0; i < 8*grargs->framesPerBuffer; ++i) {
				x += rptr[i]*cexpf(-2.0f*im*PI*i*k/acc);
			}
			int h = (int)(sqrt(creal(x)*creal(x)+cimag(x)*cimag(x)));
			mvaddch(yres-1-h, k, 'O');
		}
		refresh();
	}
}

PaError openAudioFile(audioData* data, char* name, int ch, PaStream** outStream) {
	data->file = sf_open(name, SFM_READ, &data->sfinfo);	
	if (!data->file) {
		return paNoError+1;
	}
	data->framesRemaining = data->sfinfo.frames;	
	return Pa_OpenDefaultStream(outStream, 0, ch, paFloat32, 44100, 256, outputCallback, data);
}

int main(int argc, char** argv)	{
	if (argc < 2) {
		fprintf(stderr, "Missing input filename\n");
		return 0;
	}
	if (argc > 2) {
		fprintf(stderr, "Too many arguments!\n");
		return 0;
	}
	char** playlist;
	size_t songs = 0;
	if (strncmp(argv[1], "--playlist=", 11) == 0) {
		char* plname = argv[1]+11;
		FILE* file = fopen(plname, "r");
		char line[256];
		if (file == NULL) {
			fprintf(stderr, "Couldn't open file!\n");
			return 0;
		}
		while (fgets(line, sizeof(line), file)) {
			++songs;
		}
		playlist = malloc(sizeof(char*)*songs);
		rewind(file);
		size_t i = 0;
		while (fgets(line, sizeof(line), file)) {
			size_t length = strlen(line);
			if (line[length-1] == '\n') {
				length -= 1;
			}
			playlist[i] = (char*)calloc(length, sizeof(char));
			memcpy(playlist[i], line, length);
			++i;
		}
		fclose(file);
	}
	else {
		playlist = malloc(sizeof(char*)*1);
		playlist[0] = argv[1];
		songs = 0;				//no need to free stack memory
	}
	paused = false;
	audioData data;
	data.file = NULL;
	data.buffer = (float*)malloc(256*8*sizeof(float));
	if (data.buffer == NULL) {
		fprintf(stderr, "Couldn't open file!\n");
		return 0;
	}
	PaError	err = Pa_Initialize();
	if (err	!= paNoError) {
		fprintf(stderr,	"PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		return 0;
	}
	int nDev = Pa_GetDeviceCount();
	if (nDev <= 0) {
		fprintf(stderr, "Sorry! Couldn't find any devices! Pa_CountDevices returned 0x%x\n", nDev);
		free(data.buffer);
		Pa_Terminate();
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
			free(playlist);
		}
		return 0;
	}
	const PaDeviceInfo *devInfo;
	for (int i = 0; i < nDev; ++i) {
		devInfo = Pa_GetDeviceInfo(i);
		printf("Device %d : %s\n", i+1, devInfo->name);
	}
	int ch = 0;
	printf("Enter the number of the output device you'd like to use:\n");
	scanf("%d", &ch);
	ch--; 
	size_t curSong = 0;
	PaStream* outStream;
	err = openAudioFile(&data, playlist[0], ch, &outStream);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
			free(playlist);
		}
		Pa_Terminate();
		return 0;
	}
	printf("Opened!\n");
	initscr();
	pthread_t graph_id;
	graphArgs gr_args;
	gr_args.framesPerBuffer = 256;
	gr_args.userData = &data;
	pthread_create(&graph_id, NULL, graphOut, &gr_args);
	acc = 2*getmaxx(stdscr);
	yres = getmaxy(stdscr);
	err = Pa_StartStream(outStream);
	if (err != paNoError) {
		pthread_cancel(graph_id);
		endwin();
		fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
			free(playlist);
		}
		Pa_Terminate();
		return 0;
	}
	char ctrl;
	while ((ctrl = getch()) != 27 && ctrl != 'q') {
		switch(ctrl) {
			case ' ':
				if (!paused) {
					err = Pa_StopStream(outStream);
				}
				else {
					err = Pa_StartStream(outStream);
				}
				if (err == paNoError) {
					paused = !paused;
				}
				break;
			case 'n':
				if (curSong >= songs-1) {
					break;
				}
				err = Pa_StopStream(outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
						free(playlist);
					}
					Pa_Terminate();
					return 0;
				}
				err = openAudioFile(&data, playlist[++curSong], ch, &outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
						free(playlist);
					}
					Pa_Terminate();
					return 0;
				}
				err = Pa_StartStream(outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
						free(playlist);
					}
					Pa_Terminate();
					return 0;
				}
				break;
			case 'p':
				if (curSong <= 0) {
					break;
				}
				err = Pa_StopStream(outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
						free(playlist);
					}
					Pa_Terminate();
					return 0;
				}
				err = openAudioFile(&data, playlist[--curSong], ch, &outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
						free(playlist);
					}
					Pa_Terminate();
					return 0;
				}
				err = Pa_StartStream(outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
						free(playlist);
					}
					Pa_Terminate();
					return 0;
				}
				break;

		}
	}
	err = Pa_StopStream(outStream);
	if (err	!= paNoError) {
		pthread_cancel(graph_id);
		endwin();
		fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
			free(playlist);
		}
		Pa_Terminate();
		return 0;
	}
	Pa_CloseStream(outStream);
	pthread_cancel(graph_id);
	err = Pa_Terminate();
	if (err	!= paNoError) {
		endwin();
		fprintf(stderr,	"PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
			free(playlist);
		}
		return 0;
	}
	endwin();
	free(data.buffer);
}
