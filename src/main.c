#include <time.h>
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
#include <errno.h>

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
	uint8_t mode;			//0 for winAmp-like, 1 for cool logo
	void* userData;
	char* logoFile;
	char** logoText;
	int* x;
	int* y;
	int h;
} graphArgs;

void* graphOut(void *args) {
	graphArgs *grargs = (graphArgs*)args;
	printf("%d channels\n", ((audioData*)(grargs->userData))->sfinfo.channels);
	double ang = M_PI/4;
	int w, h;
	getmaxyx(stdscr, h, w);
	grargs->h = h;
	FILE* logo = fopen(grargs->logoFile, "r");
	grargs->logoText = malloc(h*sizeof(char*));
	char** logoText = grargs->logoText;
	for (int i = 0; i < h; ++i) {
		logoText[i] = malloc(sizeof(char)*w);	
	}
	int line = 0;
	int cnt = 0;
	int sz = 0;
	if (logo) {
		while (fgets(logoText[line], w, logo)) {
			for (int i = 0; i < w; ++i) {
				if (logoText[line][i] != ' ' && logoText[line][i] != 0) {
					++cnt;
					if (sz < i+1) {
						sz = i+1;
					}
				}
			}
			++line;
		}
		grargs->x = malloc(sizeof(int)*cnt);
		grargs->y = malloc(sizeof(int)*cnt);
		int cur_num = 0;
		for (int i = 0; i < line; ++i) {
			for (int j = 0; j < sz; ++j) {
				if (logoText[i][j] != ' ' && logoText[i][j] != 0) {
					grargs->x[cur_num] = j-sz/2;
					grargs->y[cur_num] = i-line/2;
					++cur_num;
				}
			}
		}
	}
	printf("%d %d %d", line, cnt, sz);
	int stdist = 3*sz/2;
	int mid_h = h/2;
	int mid_w = w/2;
	struct timespec rawtime;
	clock_gettime(CLOCK_MONOTONIC_RAW, &rawtime);
	while (true) {
		if (paused) {
			continue;
		}
		float *rptr = ((audioData*)(grargs->userData))->buffer;	
		clear();
		int32_t intensity = 0;
		for (size_t k = 0; k < acc/2; ++k) {
			float complex im = CMPLX(0,1);
			float complex x = 0;	
			for (int i = 0; i < ((audioData*)(grargs->userData))->sfinfo.channels*((audioData*)(grargs->userData))->sfinfo.channels*((audioData*)(grargs->userData))->sfinfo.channels*grargs->framesPerBuffer; ++i) {
				x += rptr[i]*cexpf(-2.0f*im*PI*i*(((1-grargs->mode)*acc/4+k)%(acc/2))/acc);
			}
			int h = (int)(sqrt(creal(x)*creal(x)+cimag(x)*cimag(x)));
			mvaddch(yres-1-h, k, 'O');
			intensity += h;
		}
		if (grargs->mode) {
			struct timespec curtime;
			clock_gettime(CLOCK_MONOTONIC_RAW, &curtime);
			int32_t elapsed = (curtime.tv_sec - rawtime.tv_sec)*1000+(curtime.tv_nsec - rawtime.tv_nsec)/1000000;
			for (int i = 0; i < cnt; ++i) {
                        	int x_cur, y_cur;
                        	x_cur = mid_w+(int)(grargs->x[i]*sin(ang)); 
                        	y_cur = mid_h+(int)(grargs->y[i]*(stdist-grargs->x[i]*cos(ang))/stdist);
                        	mvaddch(y_cur, x_cur, '8');
                	}
                	ang += elapsed*0.001*intensity/acc;
			rawtime = curtime;
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
	return Pa_OpenDefaultStream(outStream, 0, data->sfinfo.channels, paFloat32, data->sfinfo.samplerate, 256, outputCallback, data);
}

int main(int argc, char** argv)	{
	if (argc < 2) {
		fprintf(stderr, "Missing input filename\n");
		return 0;
	}
	srand(time(NULL));
	char** playlist;
	size_t songs = 0;	//allocated memory
	size_t plSize = 0;
	bool list = false, random = false;
	graphArgs gr_args;
	gr_args.mode = 1;
	gr_args.logoFile = "logo";
	for (int arg = 1; arg < argc; ++arg) {
		if (strncmp(argv[arg], "--playlist=", 11) == 0) {
			list = true;
			char* plname = argv[arg]+11;
			FILE* file = fopen(plname, "r");
			char line[256];
			if (file == NULL) {
				fprintf(stderr, "Couldn't open file!\n");
				return 0;
			}
			while (fgets(line, sizeof(line), file)) {
				++songs;
			}
			plSize = songs;
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
			continue;
		}
		if (strncmp(argv[arg], "--logo=", 7) == 0) {
			gr_args.logoFile = argv[arg]+7;
			printf(gr_args.logoFile);
			continue;
		}
		if (strncmp(argv[arg], "-c", 2) == 0) {
			gr_args.mode = 0;
		}
		if (strncmp(argv[arg], "-r", 2) == 0) {
			random = true;
		}
	}
	if (!list) {
		playlist = malloc(sizeof(char*)*1);
		playlist[0] = argv[argc-1];
		songs = 0;				//no need to free stack memory
		plSize = 1;
	}
	if (argc < 2) {
		fprintf(stderr, "Incorrect usage! If you want to play a single song, type player *songname*\n");
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
		}
		free(playlist);
		return 0;
	}
	paused = false;
	audioData data;
	data.file = NULL;
	data.buffer = (float*)malloc(256*8*sizeof(float));
	for (size_t i = 0; i < plSize; ++i) {
		FILE* audiofile = fopen(playlist[i], "r");
		if (audiofile == NULL || errno != 0) {
			fprintf(stderr, "Couldn't open file %s!\n", playlist[i]);
			for (size_t j = 0; j < songs; ++j) {
				free(playlist[j]);
			}
			free(playlist);
			return 0;
		}
		fclose(audiofile);
	}
	if (data.buffer == NULL) {
		fprintf(stderr, "Couldn't open file!\n");
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
		}
		free(playlist);
		return 0;
	}
	PaError	err = Pa_Initialize();
	if (err	!= paNoError) {
		fprintf(stderr,	"PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
		}
		free(playlist);
		return 0;
	}
	int nDev = Pa_GetDeviceCount();
	if (nDev <= 0) {
		fprintf(stderr, "Sorry! Couldn't find any devices! Pa_CountDevices returned 0x%x\n", nDev);
		free(data.buffer);
		Pa_Terminate();
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
		}
		free (playlist);
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
	err = openAudioFile(&data, playlist[random ? rand() % songs: 0], ch, &outStream);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		for (size_t i = 0; i < songs; ++i) {
			free(playlist[i]);
		}
		free(playlist);
		Pa_Terminate();
		return 0;
	}
	printf("Opened!\n");
	initscr();
	pthread_t graph_id;	
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
		}
		free(playlist);
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
					}
					free(playlist);
					Pa_Terminate();
					return 0;
				}
				err = openAudioFile(&data, playlist[random ? rand()%songs : ++curSong], ch, &outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
					}
					free(playlist);
					Pa_Terminate();
					return 0;
				}
				err = Pa_StartStream(outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
					}
					free(playlist);
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
					}
					free(playlist);
					Pa_Terminate();
					return 0;
				}
				err = openAudioFile(&data, playlist[random ? rand()%songs : --curSong], ch, &outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);
					}
					free(playlist);
					Pa_Terminate();
					return 0;
				}
				err = Pa_StartStream(outStream);
				if (err != paNoError) {
					fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
					free(data.buffer);
					for (size_t i = 0; i < songs; ++i) {
						free(playlist[i]);	
					}
					free(playlist);
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
		}
		free(playlist);
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
		}
		free(playlist);
		return 0;
	}
	endwin();
	free(data.buffer);
	for (size_t i = 0; i < songs; ++i) {
		free(playlist[i]);	
	}
	free(playlist);
	for (int i = 0; i < gr_args.h; ++i) {
		free(gr_args.logoText[i]);
	}
	free(gr_args.logoText);
}
