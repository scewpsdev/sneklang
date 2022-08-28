#include "Input.h"


Input CreateInput(const char* src, const char* filename) {
	Input input = {};

	input.buffer = src;

	InputState state = {};
	state.line = 1;
	state.col = 1;

	state.filename = filename;

	state.buffer = input.buffer;
	state.index = 0;
	state.ptr = input.buffer;

	input.state = state;

	return input;
}

void DestroyInput(Input* input) {
}

const char* InputGetPtr(Input* input, int offset) {
	return &input->state.buffer[input->state.index + offset];
}

char InputNext(Input* input) {
	char c = input->state.buffer[input->state.index++];
	input->state.ptr++;
	if (c == '\n') {
		input->state.line++;
		input->state.col = 1;
	}
	else {
		input->state.col++;
	}
	return c;
}

char InputPeek(Input* input, int offset) {
	return input->state.buffer[input->state.index + offset];
}

bool InputHasNext(Input* input) {
	return input->state.buffer[input->state.index] != 0;
}
