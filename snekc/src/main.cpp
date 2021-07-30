#include "snek.h"

#include "file.h"
#include "c_backend.h"

#include <stdio.h>
#include <stdarg.h>


#define max(a, b) ((a) > (b) ? (a) : (b))


static char* GetModuleNameFromPath(const char* path)
{
	const char* forwardSlash = strrchr(path, '/');
	const char* backwardSlash = strrchr(path, '\\');
	const char* slash = (forwardSlash && backwardSlash) ? (char*)max((int)forwardSlash, (int)backwardSlash) : forwardSlash ? forwardSlash : backwardSlash ? backwardSlash : NULL;
	if (slash)
		slash++;
	else
		slash = path;

	const char* dot = strrchr(slash, '.');
	if (!dot)
		dot = slash + strlen(slash);

	int length = (int)(dot - slash);

	char* name = new char[length + 1];
	strncpy(name, slash, length);
	name[length] = 0;

	return name;
}

static void OnCompilerMessage(MessageType msgType, const char* msg, ...)
{
	va_list args;
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);
}

int main(int argc, char* argv[])
{
	SkContext* context = CreateSnekContext(OnCompilerMessage);

	const char* buildFolder = ".";
	const char* filename = "a.exe";

	for (int i = 1; i < argc; i++)
	{
		const char* arg = argv[i];
		if (strlen(arg) > 0 && arg[0] == '-')
		{
			if (strcmp(arg, "-build") == 0)
			{
				if (i < argc - 1)
				{
					arg = argv[++i];
					buildFolder = arg;
				}
				else
				{
					// TODO ERROR
					SnekAssert(false, "");
				}
			}
			else
			{
				// TODO ERROR
				SnekAssert(false, "");
			}
		}
		else
		{
			const char* srcPath = argv[i];
			char* moduleName = GetModuleNameFromPath(srcPath);
			if (char* src = LoadTextFile(srcPath))
			{
				SnekAddSource(context, src, moduleName);
			}
			else
			{
				// TODO ERROR
				SnekAssert(false, "");
			}
		}
	}

	if (SnekRunParser(context))
	{
		if (SnekRunResolver(context))
		{
			printf("Generating code...\n");
			CBackend* cb = CreateCBackend(context);
			if (CBackendCompile(cb, context->asts.buffer, context->asts.size, filename, buildFolder))
			{
				printf("Build successful!\n");
			}
			DestroyCBackend(cb);
		}
		else
		{
			printf("Analyzer failed!\n");
		}
	}
	else
	{
		printf("Parser failed!\n");
	}

	DestroySnekContext(context);
}
