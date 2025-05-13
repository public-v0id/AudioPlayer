#ifndef ALPHABET_H
#define ALPHABET_H

typedef struct {
	const int* x;
	const int* y;
	int sz;
} letter;

extern letter alphabet[256];

void alphabet_init();

#endif
