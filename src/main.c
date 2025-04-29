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
#include <dirent.h>

#define TABLE_SIZE 200
#define PI (3.14159265)
#define CHANNELS 2
typedef struct {
	SNDFILE* file;
	SF_INFO sfinfo;
	float* buffer;
	long framesRemaining;
} audioData;

typedef struct playlist_str {
	struct playlist_str* next;
	struct playlist_str* prev;
	char* song;
} playlist_t;

size_t acc, yres;
volatile bool paused;
volatile int curBtn;
int btnCnt;

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
	int btn;
} graphArgs;

void* graphOut(void *args) {
	graphArgs *grargs = (graphArgs*)args;
	double ang = M_PI/4;
	int w, h;
	getmaxyx(stdscr, h, w);
	grargs->h = h;
	char** logoText;
	char btn[4][11] = {"[ ][Quit]", "[ ][Prev]", "[ ][Pause]", "[ ][Next]"};
	btnCnt = 4;
	int space = (w-28)/4;
	curBtn = 2;
	int cnt = 0;
	int sz = 0;	
	if (grargs->logoFile != NULL) {
		FILE* logo = fopen(grargs->logoFile, "r");
		grargs->logoText = malloc(h*sizeof(char*));
		logoText = grargs->logoText;
		for (int i = 0; i < h; ++i) {
			logoText[i] = malloc(sizeof(char)*w);	
		}	
		int line = 0;
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
	else if (grargs->mode == 1) {
		fprintf(stderr, "No logo file\n");
		grargs->logoText = malloc(sizeof(char*));
		grargs->logoText[0] = malloc(sizeof(char));
		{
			int tmp[] = {-67, -66, -65, -64, -63, -62, -61, -60, -55, -54, -47, -46, -43, -42, -41, -40, -39, -38, -31, -30, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -4, -3, -2, -1, 0, 1, 18, 19, 28, 29, 34, 35, 36, 37, 38, 39, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, -67, -66, -60, -59, -55, -54, -47, -46, -43, -42, -37, -36, -31, -30, -15, -14, -5, -4, 1, 2, 18, 19, 28, 29, 33, 34, 39, 40, 48, 49, 56, 57, 65, 66, -67, -66, -63, -62, -59, -58, -55, -54, -47, -46, -43, -42, -40, -39, -36, -35, -31, -30, -15, -14, -6, -5, 2, 3, 18, 19, 28, 29, 33, 34, 39, 40, 48, 49, 56, 57, 60, 61, 62, 66, 67, -67, -66, -63, -62, -59, -58, -55, -54, -47, -46, -43, -42, -37, -36, -31, -30, -15, -14, -7, -6, 18, 19, 28, 29, 32, 33, 38, 39, 40, 41, 48, 49, 56, 57, 60, 62, 63, 66, 67, -67, -66, -60, -59, -55, -54, -47, -46, -43, -42, -41, -40, -39, -38, -37, -31, -30, -15, -14, -7, -6, 19, 20, 27, 28, 32, 33, 37, 38, 40, 41, 48, 49, 56, 57, 60, 63, 66, 67, -67, -66, -65, -64, -63, -62, -61, -60, -55, -54, -47, -46, -43, -42, -36, -35, -31, -30, -15, -14, -7, -6, 19, 20, 27, 28, 32, 33, 36, 37, 40, 41, 48, 49, 56, 57, 60, 63, 66, 67, -67, -66, -55, -54, -47, -46, -43, -42, -39, -38, -35, -34, -31, -30, -15, -14, -7, -6, 19, 20, 27, 28, 32, 33, 35, 36, 40, 41, 48, 49, 56, 57, 60, 62, 63, 66, 67, -67, -66, -55, -54, -47, -46, -43, -42, -39, -38, -35, -34, -31, -30, -15, -14, -6, -5, 2, 3, 20, 21, 26, 27, 33, 34, 35, 39, 40, 48, 49, 56, 57, 60, 61, 62, 66, 67, -67, -66, -54, -53, -47, -46, -43, -42, -36, -35, -31, -30, -15, -14, -5, -4, 1, 2, 21, 22, 25, 26, 33, 34, 39, 40, 48, 49, 56, 57, 65, 66, -67, -66, -53, -52, -51, -50, -49, -48, -47, -46, -43, -42, -41, -40, -39, -38, -37, -36, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -4, -3, -2, -1, 0, 1, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 22, 23, 24, 25, 34, 35, 36, 37, 38, 39, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65};
			grargs->x = malloc(sizeof(int)*450);
			memcpy(grargs->x, tmp, sizeof(int)*450);
		}
		{
			int tmp[] = {-5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
			grargs->y = malloc(sizeof(int)*450);
			memcpy(grargs->y, tmp, sizeof(int)*450);
		}
		grargs->h = 1;
		sz = 135;
		cnt = 450;
	}
	else if (grargs->mode == 0) {
		grargs->logoText = malloc(sizeof(char*));
		grargs->logoText[0] = malloc(sizeof(char));
		grargs->x = malloc(sizeof(int));
		grargs->y = malloc(sizeof(int));
		grargs->h = 0;
	}
	int stdist = 3*sz/2;
	int mid_h = h/2;
	int mid_w = w/2;
	struct timespec rawtime;
	clock_gettime(CLOCK_MONOTONIC_RAW, &rawtime);
	while (true) {	
		if (paused) {
			clear();	
			for (int i = 0; i < acc/2; ++i) {
				mvaddch(yres-6-(1-grargs->mode)*yres/2, i, 'O');
			}
			if (grargs->mode) {
				for (int i = 0; i < cnt; ++i) {
	                        	int x_cur, y_cur;
        	                	x_cur = mid_w+(int)(grargs->x[i]*sin(ang)); 
                	        	y_cur = mid_h+(int)(grargs->y[i]*(stdist-grargs->x[i]*cos(ang))/stdist);
                        		mvaddch(y_cur, x_cur, '8');
				}
			}
			int curC = space;
			btn[curBtn][1] = 'X';
			move (1, 2);
			printw(btn[0]);
			btn[0][1] = ' ';
			for (int i = 1; i < btnCnt; ++i) {
				move(h-2, curC);
			       	printw(btn[i]);
				btn[i][1] = ' ';
				curC += space + strlen(btn[i]);
			}
			refresh();
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
			int h = (int)(sqrt(creal(x)*creal(x)+cimag(x)*cimag(x)))/(2-grargs->mode);
			if (!grargs->mode) {
				mvaddch(yres-6+h-yres*(1-grargs->mode)/2, k,
						'O'); }
			mvaddch(yres-6-h-yres*(1-grargs->mode)/2, k, 'O');
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
		int curC = space;
		btn[curBtn][1] = 'X';
		move (1, 2);
		printw(btn[0]);
		btn[0][1] = ' ';
		for (int i = 1; i < btnCnt; ++i) {
			move(h-2, curC);
		       	printw(btn[i]);
			btn[i][1] = ' ';
			curC += space + strlen(btn[i]);
		}
		refresh();
	}
}

void pl_free(playlist_t* pl) {
	if (pl == NULL) {
		return;
	}
	playlist_t* next = pl->next;
	while (next != NULL) {
		playlist_t* next_n = next->next;
		free(next->song);
		free(next);
		next = next_n;
	}
	playlist_t* prev = pl->prev;
	while (prev != NULL) {
		playlist_t* prev_p = prev->prev;
		free(prev->song);
		free(prev);
		prev = prev_p;
	}
	free(pl->song);
	free(pl);
}

PaError openAudioFile(audioData* data, char* name, int ch, PaStream** outStream) {
	data->file = sf_open(name, SFM_READ, &data->sfinfo);	
	if (!data->file) {
		return paNoError+1;
	}
	data->framesRemaining = data->sfinfo.frames;	
	return Pa_OpenDefaultStream(outStream, 0, data->sfinfo.channels, paFloat32, data->sfinfo.samplerate, 256, outputCallback, data);
}

void shuffle(playlist_t* list, int n) {
	if (n == 1) {
		return;
	}
	for (int i = 0; i < n; ++i) {
		int f = rand()%n;
		int s = rand()%n;
		while (f == s) {
			f = rand()%n;
		}
		playlist_t* first = list;
		playlist_t* second = list;
		playlist_t* iter = list;
		for (int i = 0; i < f || i < s || i < n; ++i) {
			if (i == f) {
				first = iter;
			}
			if (i == s) {
				second = iter;
			}
			iter = iter->next;
		}
		char* h = first->song;
		first->song = second->song;
		second->song = h;
	}
}

int main(int argc, char** argv)	{	
	srand(time(NULL));
	playlist_t* playlist_b = NULL;
	playlist_t* playlist_e = NULL;
	playlist_t* curSong = NULL;
	size_t songs = 0;	//allocated memory
//	size_t plSize = 0;
	bool list = false, random = false;
	graphArgs gr_args;
	gr_args.mode = 1;
	gr_args.logoFile = NULL;
	char* songname = NULL;
	for (int arg = 1; arg < argc; ++arg) {
		if (strncmp(argv[arg], "--playlist=", 11) == 0) {
			list = true;
			char* plname = argv[arg]+11;
			fflush(stdout);
			FILE* file = fopen(plname, "r");
			if (file == NULL) {
				fprintf(stderr, "Couldn't open file!\n");
				return 0;
			}
			char line[256];	
			while (fgets(line, sizeof(line), file)) {
				playlist_t* new = malloc(sizeof(playlist_t));
				if (new == NULL) {
					fclose(file);
					fprintf(stderr, "Unable to allicate playlist!\n");
					return 0;
				}
				new->song = calloc(strlen(line), sizeof(char));
				new->next = NULL;
				new->prev = NULL;
				if (line[strlen(line)-1] == '\n') {
					line[strlen(line)-1] = 0;
				}
				memmove(new->song, line, strlen(line));
				if (playlist_b == NULL) {
					playlist_b = new;
					playlist_e = new;
				}
				else {
					playlist_e->next = new;
					new->prev = playlist_e;
					playlist_e = new;
				}
				++songs;
			}
			fclose(file);
			continue;
		}
		if (strncmp(argv[arg], "--logo=", 7) == 0) {
			gr_args.logoFile = argv[arg]+7;
			continue;
		}
		if (strncmp(argv[arg], "-c", 2) == 0) {
			gr_args.mode = 0;
			continue;
		}
		if (strncmp(argv[arg], "-r", 2) == 0) {
			random = true;
			continue;
		}
		songname = argv[arg];
	}
	if (!list && !songname) {
		DIR* dir;
		struct dirent* entry;
		dir = opendir("./");
		if (dir == NULL) {
			fprintf(stderr, "Unable to open current catalogue!\n");
			pl_free(playlist_b);
			return 0;
		}
		while ((entry = readdir(dir)) != NULL) {
			size_t len = strlen(entry->d_name);
			if (len > 4 && (strncmp(entry->d_name+len-4, ".mp3", 4) == 0 || strncmp(entry->d_name+len-4, ".wav", 4) == 0 || strncmp(entry->d_name+len-5, ".flac", 5) == 0)) {
				++songs;
				playlist_t* new = malloc(sizeof(playlist_t));
				if (new == NULL) {
					closedir(dir);
					fprintf(stderr, "Unable to allicate playlist!\n");
					return 0;
				}
				new->song = calloc(len, sizeof(char));
				new->next = NULL;
				new->prev = NULL;
				if (entry->d_name[len-1] == '\n') {
					entry->d_name[len-1] = 0;
				}
				memmove(new->song, entry->d_name, len);
				if (playlist_b == NULL) {
					playlist_b = new;
					playlist_e = new;
				}
				else {
					playlist_e->next = new;
					new->prev = playlist_e;
					playlist_e = new;
				}
	
			}
		}
	}	
	else if (!list) {
		playlist_b = malloc(sizeof(playlist_t)*1);
		playlist_b->song = calloc(strlen(songname), sizeof(char));
		memmove(playlist_b->song, songname, strlen(songname));
		songs = 1;
	}
	paused = false;
	audioData data;
	data.file = NULL;
	data.buffer = (float*)malloc(256*8*sizeof(float));
	if (data.buffer == NULL) {
		fprintf(stderr, "Couldn't open file!\n");
		pl_free(playlist_b);
		return 0;
	}
	PaError	err = Pa_Initialize();
	if (err	!= paNoError) {
		fprintf(stderr,	"PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		pl_free(playlist_b);
		return 0;
	}
	int nDev = Pa_GetDeviceCount();
	if (nDev <= 0) {
		fprintf(stderr, "Sorry! Couldn't find any devices! Pa_CountDevices returned 0x%x\n", nDev);
		free(data.buffer);
		Pa_Terminate();
		pl_free(playlist_b);
		return 0;
	}
	const PaDeviceInfo *devInfo;
	for (int i = 0; i < nDev; ++i) {
		devInfo = Pa_GetDeviceInfo(i);
		printf("Device %d : %s\n", i+1, devInfo->name);
	}
	if (random) {
		shuffle(playlist_b, songs);
	}
	int ch = 0;
	printf("Enter the number of the output device you'd like to use:\n");
	scanf("%d", &ch);
	ch--; 
	PaStream* outStream;
	curSong = playlist_b;
	err = openAudioFile(&data, curSong->song, ch, &outStream);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		pl_free(playlist_b);
		Pa_Terminate();
		return 0;
	}
	initscr();
	noecho();
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
		pl_free(playlist_b);
		Pa_Terminate();
		return 0;
	}
	keypad(stdscr, TRUE);
	int ctrl;
	while (((ctrl = getch()) != 'q') && (ctrl != ' ' || curBtn != 0)) {
		switch(ctrl) {
			case KEY_LEFT:
				if (curBtn > 0) {
					curBtn--;
				}
				break;
			case KEY_RIGHT:
				if (curBtn < btnCnt-1) {
					++curBtn;
				}
				break;
			case ' ':
				switch (curBtn) {
					case 2: //pause
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
					case 3: //next
						if (curSong == playlist_e || curSong->next == NULL) {
							break;
						}
//						fprintf(stdout, "Moving to next\n");
						if (!paused) {
							err = Pa_StopStream(outStream);
							if (err != paNoError) {
								fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
								free(data.buffer);
								pl_free(playlist_b);
								endwin();
								Pa_Terminate();
								return 0;
							}
						}
//						fprintf(stdout, "Stopped prev\n");
						curSong = curSong->next;
//						fprintf(stdout, "Switched song\n");
						err = openAudioFile(&data, curSong->song, ch, &outStream);
						if (err != paNoError) {
							fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
							free(data.buffer);
							pl_free(playlist_b);
							endwin();
							Pa_Terminate();
							return 0;
						}
						fprintf(stdout, "Opened file\n");
						paused = false;
						err = Pa_StartStream(outStream);
//						fprintf(stdout, "Started stream\n");
						if (err != paNoError) {
							fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
							free(data.buffer);
							pl_free(playlist_b);
							endwin();
							Pa_Terminate();
							return 0;
						}
					break;
					case 1: //prev
						if (curSong == playlist_b || curSong->prev == NULL) {
							break;
						}
						if (!paused) {
							err = Pa_StopStream(outStream);
							if (err != paNoError) {
								fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
								free(data.buffer);
								pl_free(playlist_b);
								endwin();
								Pa_Terminate();
								return 0;
							}
						}
						curSong = curSong->prev;
						err = openAudioFile(&data, curSong->song, ch, &outStream);
						if (err != paNoError) {
							fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
							free(data.buffer);	
							pl_free(playlist_b);
							endwin();
							Pa_Terminate();
							return 0;
						}
						paused = false;
						err = Pa_StartStream(outStream);
						if (err != paNoError) {
							fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
							free(data.buffer);
							pl_free(playlist_b);
							endwin();
							Pa_Terminate();
							return 0;
						}
					break;
				}
			break;
		}
	}
	if (!paused) {
		err = Pa_StopStream(outStream);
		if (err	!= paNoError) {
			pthread_cancel(graph_id);
			endwin();
			fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
			free(data.buffer);
			pl_free(playlist_b);
			Pa_Terminate();
			return 0;
		}
	}
	Pa_CloseStream(outStream);
	pthread_cancel(graph_id);
	while (pthread_join(graph_id, NULL) != 0) {
		printf("Waiting\n");
	}
	err = Pa_Terminate();
	if (err	!= paNoError) {
		endwin();
		fprintf(stderr,	"PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		pl_free(playlist_b);
		return 0;
	}
	endwin();
//	printf("Freeing buffer\n");
	free(data.buffer);
//	printf("Freeing playlist\n");
	pl_free(playlist_b);
//	printf("Freeing logotext\n");
	for (int i = 0; i < gr_args.h; ++i) {
		free(gr_args.logoText[i]);
	}
	free(gr_args.logoText);
//	printf("Freeing xy\n");
	free(gr_args.x);
	free(gr_args.y);
}
