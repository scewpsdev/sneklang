#include "file.h"

#include <stdio.h>
#include <Windows.h>


char* LoadTextFile(const char* path)
{
	FILE* file = fopen(path, "rt");
	if (!file)
		return NULL;

	fseek(file, 0, SEEK_END);
	size_t length = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* text = new char[length + 1];
	length = fread(text, 1, length, file);
	text[length] = '\0';
	fclose(file);

	return text;
}

bool WriteTextFile(const char* str, const char* path)
{
	FILE* file = fopen(path, "w");
	if (!file)
		return false;

	fputs(str, file);
	fclose(file);

	return true;
}

char* GetExecutablePath()
{
	char* buffer = new char[64];
	GetModuleFileNameA(NULL, (LPSTR)buffer, 64);
	return buffer;
}
