#ifndef ALPHABET_H
#define ALPHABET_H

typedef struct {
	const int* x;
	const int* y;
	int sz;
} letter;

extern letter alphabet[2048];

void alphabet_init();

#endif
