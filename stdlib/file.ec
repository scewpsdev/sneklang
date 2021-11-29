module file;
namespace File;

import stdc;


public string ReadText(string path)
{
	FILE* file = fopen(path, "r");
	if (file == null)
	{
		fprintf(stderr, "Failed to read text file '%s'\n", path);
		return null;
	}

	fseek(file, 0, SEEK_END);
	int length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* buffer = new char:(length + 1);
	if (fread(buffer, 1, (size_t)length, file) == length)
	{
		buffer[length] = '\0';
		fclose(file);

		return buffer;
	}
	else
	{
		free buffer;
		fclose(file);
		return null;
	}
}

public byte* ReadBinary(string path, int* out_length)
{
	FILE* file = fopen(path, "rb");
	if (file == null)
	{
		fprintf(stderr, "Failed to read binary file '%s'\n", path);
		return null;
	}

	fseek(file, 0, SEEK_END);
	int length = ftell(file);
	fseek(file, 0, SEEK_SET);

	byte* buffer = new byte:(length);
	if (fread(buffer, 1, (size_t)length, file) == length)
	{
		fclose(file);
		*out_length = length;

		return buffer;
	}
	else
	{
		free buffer;
		fclose(file);
		return null;
	}
}
