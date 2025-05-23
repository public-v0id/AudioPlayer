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
#include "alphabet.h"

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
	char* trackname;
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
	volatile int cnt = 0;
	int sz = 0;	
	char* bandname = NULL;
	if (grargs->logoFile != NULL) {
//		fprintf(stderr, "opening\n");
		FILE* logo = fopen(grargs->logoFile, "r");
		grargs->logoText = malloc(h*sizeof(char*));
		if (grargs->logoText == NULL) {
			printf("Malloc error!\n");
		}
//		fprintf(stderr, "malloc\n");
		logoText = grargs->logoText;
		for (int i = 0; i < h; ++i) {
			logoText[i] = malloc(sizeof(char)*w);	
			if (logoText[i] == NULL) {
				printf("Malloc error! %d\n", i);
			}
		}	
		int line = 0;
		while (line < h-10 && fgets(logoText[line], w, logo)) {
//			fprintf(stderr, "%d", line);
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
//		fprintf(stderr, "two other mallocs\n");
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
		grargs->h = 1;
		bandname = grargs->trackname;	
		int len = 0;
		char* spl = strchr(bandname, '-');
		if (spl == NULL) {	
			bandname = "Unknown";
			len = strlen(bandname);
			goto out;
		}
		if (spl <= bandname) {
			bandname = "Unknown";
			len = strlen(bandname);
			goto out;
		}
		while (*spl == '-' || *spl == ' ') {
			--spl;
		}
		if (spl < bandname) {
			bandname = "Unknown";
			len = strlen(bandname);
			goto out;
		}
		len = (spl-bandname)+1;
out:
		sz = len*10;
		cnt = 0;	
		for (int i = 0; i < len; ++i) {
//			printf("%c\n", bandname[i]);
			cnt += alphabet[(unsigned int)bandname[i]].sz;
		}
//		printf("%s %d %d\n", bandname, sz, cnt);
		grargs->x = malloc(sizeof(int)*cnt);
		grargs->y = malloc(sizeof(int)*cnt);
		int cur = 0;
		for (int i = 0; i < len; ++i) {
			for (int j = 0; j < alphabet[(unsigned int)bandname[i]].sz; ++j) {
				grargs->x[cur] = alphabet[(unsigned int)bandname[i]].x[j]+10*i-5*len;
				grargs->y[cur] = alphabet[(unsigned int)bandname[i]].y[j]-5;
				++cur;		
			}
		}
		if (cnt >= 1) {
//			printf("%d\n", grargs->x[0]);
		}
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
			printw("%s", btn[0]);
			btn[0][1] = ' ';
			for (int i = 1; i < btnCnt; ++i) {
				move(h-2, curC);
			       	printw("%s", btn[i]);
				btn[i][1] = ' ';
				curC += space + strlen(btn[i]);
			}
			refresh();
			continue;
		}
		if (bandname != NULL && bandname != grargs->trackname) {
			free(grargs->x);
			free(grargs->y);
			char* bandname = grargs->trackname;
			while (strchr(bandname, '/') != NULL) {
				bandname = strchr(bandname, '/')+1;
			}	
			int len = 0;
			char* spl = strchr(bandname, '-');
			if (spl == NULL) {	
				bandname = "Unknown";
				len = strlen(bandname);
				goto out1;
			}
			if (spl <= bandname) {
				bandname = "Unknown";
				len = strlen(bandname);
				goto out1;
			}
			while (*spl == '-' || *spl == ' ') {
				--spl;
			}
			if (spl < bandname) {
				bandname = "Unknown";
				len = strlen(bandname);
				goto out1;
			}
			len = (spl-bandname)+1;
out1:
			sz = len*10;
			cnt = 0;
			for (int i = 0; i < len; ++i) {
				cnt += alphabet[(unsigned int)bandname[i]].sz;
			}
			grargs->x = malloc(sizeof(int)*cnt);
			grargs->y = malloc(sizeof(int)*cnt);
			int cur = 0;
			for (int i = 0; i < len; ++i) {
				for (int j = 0; j < alphabet[(unsigned int)bandname[i]].sz; ++j) {
					grargs->x[cur] = alphabet[(unsigned int)bandname[i]].x[j]+10*i-5*len;
					grargs->y[cur] = alphabet[(unsigned int)bandname[i]].y[j]-5;
					++cur;			
				}
			}	
//			printf("%d\n", grargs->x[0]);
			stdist = 3*sz/2;
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
				double x_coef = 0.75*w/sz < 1 ? 0.75*w/sz : 1;
                        	x_cur = mid_w+(int)((grargs->x[i]*sin(ang))*x_coef); 
                        	y_cur = mid_h+(int)(grargs->y[i]*(stdist-grargs->x[i]*cos(ang))/stdist);
                        	mvaddch(y_cur, x_cur, '8');
                	}
                	ang += elapsed*0.001*intensity/acc;
			rawtime = curtime;
		}
		int curC = space;
		btn[curBtn][1] = 'X';
		move (1, 2);
		printw("%s", btn[0]);
		btn[0][1] = ' ';
		for (int i = 1; i < btnCnt; ++i) {
			move(h-2, curC);
		       	printw("%s", btn[i]);
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
		fprintf(stderr, "Unable to open file!\n");
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

char* allocFilename(char* songname) {
	int move = 0;
	if (!strchr(songname, '/')) {
		move = 2;
	}
//	printf("len is %ld\n", strlen(songname));
	char* song = calloc(strlen(songname)+move+1, sizeof(char));
	memmove(song+move, songname, strlen(songname));
	if (move != 0) {
		song[0] = '.';
		song[1] = '/';
	}
	song[strlen(songname)+move] = 0;
//	printf("len is %ld\n", strlen(song));
	return song;
}

int main(int argc, char** argv)	{	
	srand(time(NULL));
	alphabet_init();	
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
			printf("%s\n", entry->d_name);
			if (len > 4 && (strncmp(entry->d_name+len-4, ".mp3", 4) == 0 || strncmp(entry->d_name+len-4, ".wav", 4) == 0 || strncmp(entry->d_name+len-5, ".flac", 5) == 0)) {
				++songs;
				printf("song\n");
				playlist_t* new = malloc(sizeof(playlist_t));
				if (new == NULL) {
					closedir(dir);
					fprintf(stderr, "Unable to allocate playlist!\n");
					return 0;
				}
				new->next = NULL;
				new->prev = NULL;
				if (entry->d_name[len-1] == '\n') {
					entry->d_name[len-1] = 0;
				}
				new->song = allocFilename(entry->d_name);
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
		playlist_b->song = allocFilename(songname);
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
	printf("%s\n", curSong->song);
	err = openAudioFile(&data, curSong->song, ch, &outStream);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
		printf("%d", err);
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
	gr_args.trackname = curSong->song;
	while (strchr(gr_args.trackname, '/') != NULL) {
		gr_args.trackname = strchr(gr_args.trackname, '/')+1;
	}	
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
						gr_args.trackname = curSong->song;
						while (strchr(gr_args.trackname, '/') != NULL) {
							gr_args.trackname = strchr(gr_args.trackname, '/')+1;
						}
						printf("%s\n", curSong->song);
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
						gr_args.trackname = curSong->song;
						while (strchr(gr_args.trackname, '/') != NULL) {
							gr_args.trackname = strchr(gr_args.trackname, '/')+1;
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
