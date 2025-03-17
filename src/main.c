#include <stdio.h>
#include <string.h>
#include <portaudio.h>
#include <math.h>
#include <ncurses.h>
#include <sndfile.h>
#include <stdlib.h>
#include <complex.h>
#include <pthread.h>

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

static int outputCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
				void *userData) {
	audioData* data = (audioData*) userData;
	float* out = (float*)outputBuffer;
	long framesToWrite = framesPerBuffer;
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
//	printf("data read\n");
	return paContinue;
}

typedef struct {
	unsigned long framesPerBuffer;
	void* userData;
} graphArgs;

void* graphOut(void *args) {
	graphArgs *grargs = (graphArgs*)args;
	while (true) {
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

int main(int argc, char** argv)	{
	if (argc < 2) {
		fprintf(stderr, "Missing input filename\n");
		return 0;
	}
	audioData data;
	data.file = NULL;
	data.buffer = malloc(256*8*sizeof(float));
	data.file = sf_open(argv[1], SFM_READ, &data.sfinfo);
	if (!data.file) {
		fprintf(stderr, "Couldn't open file!\n");
		return 0;
	}
	data.framesRemaining = data.sfinfo.frames;
	PaError	err = Pa_Initialize();
	if (err	!= paNoError) {
		fprintf(stderr,	"PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		return 0;
	}
	printf("Initialized!\n");
	int nDev = Pa_GetDeviceCount();
	if (nDev <= 0) {
		fprintf(stderr, "Sorry! Couldn't find any devices! Pa_CountDevices returned 0x%x\n", nDev);
		free(data.buffer);
		Pa_Terminate();
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
	PaStream* outStream;
	err = Pa_OpenDefaultStream(&outStream, 0, ch, paFloat32, 44100, 256, outputCallback, &data);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error! %s\n", Pa_GetErrorText(err));
		free(data.buffer);
		return 0;
	}
	initscr();
	pthread_t graph_id;
	graphArgs gr_args;
	gr_args.framesPerBuffer = 256;
	gr_args.userData = &data;
	pthread_create(&graph_id, NULL, graphOut, &gr_args);
	acc = 2*getmaxx(stdscr);
	yres = getmaxy(stdscr);
	err = Pa_StartStream(outStream);
	printf("%d\n", err);
	getch();
	err = Pa_StopStream(outStream);;
	printf("%d\n", err);
	Pa_CloseStream(outStream);
	pthread_cancel(graph_id);
	err = Pa_Terminate();
	if (err	!= paNoError) {
		fprintf(stderr,	"PortAudio error! %s\n", Pa_GetErrorText(err));
	}
//	free(inpData);
	endwin();
	free(data.buffer);
}
