#pragma once


struct InputState
{
	const char* filename;

	const char* ptr;

	const char* buffer;
	int index;

	int line, col;
};

struct Input
{
	const char* buffer;
	InputState state;
};


Input CreateInput(const char* src, const char* filename);
void DestroyInput(Input* input);

const char* InputGetPtr(Input* input, int offset = 0);

char InputNext(Input* input);
char InputPeek(Input* input, int offset);
bool InputHasNext(Input* input);
