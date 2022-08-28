#pragma warning( disable : 4302 4311 4312 )

#ifdef _DEBUG
#	define DBG_LOG(x, ...) fprintf(stderr, x, ##__VA_ARGS__)
#else
#	define DBG_LOG(x, ...)
#endif


#include "snek.h"

#include "File.h"
#include "utils/Utils.h"

#include "llvm/LLVMBackend.h"
#include "llvm/Linker.h"

#include <stdio.h>
#include <stdarg.h>

#include <filesystem>
#include <sstream>


#define CMD_LINE_ARG_BUILD_FOLDER "-build"
#define CMD_LINE_ARG_BUILD_FOLDER_2 "-obj"
#define CMD_LINE_ARG_OUTPUT_FILE "-out"
#define CMD_LINE_ARG_OUTPUT_FILE_2 "-o"
#define CMD_LINE_ARG_GEN_DEBUG_INFO "--g"
#define CMD_LINE_ARG_DEBUG "--debug"
#define CMD_LINE_ARG_RELEASE "--release"
#define CMD_LINE_ARG_LIBARY_DIR "-libpath"
#define CMD_LINE_ARG_LIBARY_DIR_2 "-L"
#define CMD_LINE_ARG_STD_LIB "--stdlib"
#define CMD_LINE_ARG_EMIT_LLVM "--emit-llvm"
#define CMD_LINE_ARG_OPT_LEVEL_1 "--o1"
#define CMD_LINE_ARG_OPT_LEVEL_2 "--o2"
#define CMD_LINE_ARG_OPT_LEVEL_3 "--o3"


char* compilerDirectory;


std::stringstream compilerMessages;


static uint64_t max(uint64_t a, uint64_t b)
{
	return a > b ? a : b;
}

static char* GetModuleNameFromPath(const char* path)
{
	const char* forwardSlash = strrchr(path, '/');
	const char* backwardSlash = strrchr(path, '\\');
	const char* slash = (forwardSlash && backwardSlash) ? (const char*)max((uint64_t)forwardSlash, (uint64_t)backwardSlash) : forwardSlash ? forwardSlash : backwardSlash ? backwardSlash : NULL;
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

static const char* GetFilenameFromPath(const char* path)
{
	const char* forwardSlash = strrchr(path, '/');
	const char* backwardSlash = strrchr(path, '\\');
	const char* slash = (forwardSlash && backwardSlash) ? (const char*)max((uint64_t)forwardSlash, (uint64_t)backwardSlash) : forwardSlash ? forwardSlash : backwardSlash ? backwardSlash : NULL;
	if (slash)
		slash++;
	else
		slash = path;

	return slash;
}

static char* GetFolderFromPath(const char* path)
{
	const char* forwardSlash = strrchr(path, '/');
	const char* backwardSlash = strrchr(path, '\\');
	const char* slash = (forwardSlash && backwardSlash) ? (const char*)max((uint64_t)forwardSlash, (uint64_t)backwardSlash) : forwardSlash ? forwardSlash : backwardSlash ? backwardSlash : NULL;
	if (!slash)
		slash = path;

	int length = (int)(slash - path);

	char* folder = new char[length + 1];
	strncpy(folder, path, length);
	folder[length] = 0;

	return folder;
}

static const char* GetExtensionFromPath(const char* path)
{
	const char* ext = strrchr(path, '.');
	if (ext)
		ext++;
	else
		ext = path + strlen(path);

	return ext;
}

static void AddSourceFile(SkContext* context, char* src, char* path, char* moduleName, char* filename, char* directory)
{
	SnekAddSource(context, src, path, moduleName, filename, directory);
}

static void AddLinkerFile(SkContext* context, const char* path)
{
	SnekAddLinkerFile(context, path);
}

static bool AddFile(SkContext* context, const char* path)
{
	const char* fileExtension = GetExtensionFromPath(path);
	if (strcmp(fileExtension, "obj") == 0 || strcmp(fileExtension, "o") == 0 || strcmp(fileExtension, "lib") == 0 || strcmp(fileExtension, "a") == 0)
	{
		AddLinkerFile(context, path);
		return true;
	}
	else
	{
		if (char* src = LoadTextFile(path))
		{
			char* moduleName = GetModuleNameFromPath(path);
			char* filename = _strdup(GetFilenameFromPath(path));
			char* directory = GetFolderFromPath(path);
			AddSourceFile(context, src, _strdup(path), moduleName, filename, directory);
			return true;
		}
		else
		{
			SnekFatal(context, ERROR_CODE_FILE_NOT_FOUND, "Unknown file '%s'", path);
			return false;
		}
	}
}

static bool AddSourceFolder(SkContext* context, const char* folder, const char* extension, bool recursive)
{
	if (!std::filesystem::exists(folder)) {
		SnekFatal(context, ERROR_CODE_FOLDER_NOT_FOUND, "Unknown folder '%s'", folder);
		return false;
	}

	bool result = true;

	for (auto& de : std::filesystem::directory_iterator(folder)) {
		std::u8string dePathStr = de.path().u8string();
		const char* dePath = (const char*)dePathStr.c_str();
		if (de.is_directory() && recursive) {
			result = AddSourceFolder(context, dePath, extension, recursive) && result;
		}
		else
		{
			const char* fileExtension = GetExtensionFromPath(dePath);
			if (fileExtension && strcmp(fileExtension, extension) == 0)
			{
				result = AddFile(context, dePath) && result;
			}
		}
	}

	return result;
}

static void OnCompilerMessage(MessageType msgType, const char* filename, int line, int col, int errCode, const char* msg, ...)
{
	static const char* const MSG_TYPE_NAMES[MESSAGE_TYPE_MAX] = {
		"<null>",
		"info",
		"warning",
		"error",
		"fatal error",
	};

	MessageType minMsgType =
#if _DEBUG
		MESSAGE_TYPE_INFO
#else
		MESSAGE_TYPE_WARNING
#endif
		;

	if (msgType >= minMsgType)
	{
		static char message[4192] = {};
		message[0] = 0;

		if (filename)
			sprintf(message + strlen(message), "%s:%d:%d: ", filename, line, col);

		if (msgType != MESSAGE_TYPE_INFO)
			sprintf(message + strlen(message), "%s: ", MSG_TYPE_NAMES[msgType]);

		va_list args;
		va_start(args, msg);
		vsprintf(message + strlen(message), msg, args);
		va_end(args);

#if _DEBUG
		fprintf(stderr, "%s\n", message);
#else
		compilerMessages << message << std::endl;
#endif
	}
}

int main(int argc, char* argv[])
{
	compilerDirectory = GetFolderFromPath(GetExecutablePath());

	SkContext* context = CreateSnekContext(OnCompilerMessage);

	const char* buildFolder = ".";
	const char* filename = "a.exe";
	bool genDebugInfo = true;
	bool emitLLVM = false;
	int optLevel = 0;

	List<const char*> additionalLibPaths = CreateList<const char*>();

	bool result = true;

	for (int i = 1; i < argc; i++)
	{
		const char* arg = argv[i];
		if (strlen(arg) > 0 && arg[0] == '-')
		{
			if (strcmp(arg, CMD_LINE_ARG_BUILD_FOLDER) == 0 || strcmp(arg, CMD_LINE_ARG_BUILD_FOLDER_2) == 0)
			{
				if (i < argc - 1)
				{
					arg = argv[++i];
					buildFolder = arg;
					if (!std::filesystem::exists(buildFolder))
					{
						if (!std::filesystem::create_directories(buildFolder))
						{
							SnekFatal(context, ERROR_CODE_CMD_ARG_SYNTAX, "Failed to create build directory '%s'", buildFolder);
							result = false;
						}
					}
				}
				else
				{
					SnekFatal(context, ERROR_CODE_CMD_ARG_SYNTAX, "Command line argument " CMD_LINE_ARG_BUILD_FOLDER " must be followed by a path");
					result = false;
				}
			}
			else if (strcmp(arg, CMD_LINE_ARG_OUTPUT_FILE) == 0 || strcmp(arg, CMD_LINE_ARG_OUTPUT_FILE_2) == 0)
			{
				if (i < argc - 1)
				{
					arg = argv[++i];
					filename = arg;

					auto outputDirectory = std::filesystem::path(filename).parent_path();

					if (!std::filesystem::exists(outputDirectory))
					{
						if (!std::filesystem::create_directories(outputDirectory))
						{
							SnekFatal(context, ERROR_CODE_CMD_ARG_SYNTAX, "Failed to create output directory '%s'", filename);
							result = false;
						}
					}
				}
				else
				{
					SnekFatal(context, ERROR_CODE_CMD_ARG_SYNTAX, "Command line argument " CMD_LINE_ARG_OUTPUT_FILE " must be followed by a path");
					result = false;
				}
			}
			else if (strcmp(arg, CMD_LINE_ARG_LIBARY_DIR) == 0 || strcmp(arg, CMD_LINE_ARG_LIBARY_DIR_2) == 0)
			{
				if (i < argc - 1)
				{
					arg = argv[++i];
					additionalLibPaths.add(arg);
				}
				else
				{
					SnekFatal(context, ERROR_CODE_CMD_ARG_SYNTAX, "Command line argument " CMD_LINE_ARG_LIBARY_DIR " must be followed by a path");
					result = false;
				}
			}
			else if (strcmp(arg, CMD_LINE_ARG_GEN_DEBUG_INFO) == 0)
			{
				genDebugInfo = true;
			}
			else if (strcmp(arg, CMD_LINE_ARG_DEBUG) == 0)
			{
				genDebugInfo = true;
				optLevel = 0;
			}
			else if (strcmp(arg, CMD_LINE_ARG_RELEASE) == 0)
			{
				genDebugInfo = false;
				optLevel = 3;
			}
			else if (strcmp(arg, CMD_LINE_ARG_STD_LIB) == 0)
			{
				std::filesystem::path compilerPath = compilerDirectory;
				std::filesystem::path srtPath = compilerPath.parent_path().parent_path();
				srtPath = srtPath.append("stdlib");
				std::string srtPathStr = srtPath.string();
				AddSourceFolder(context, srtPathStr.c_str(), "src", true);
			}
			else if (strcmp(arg, CMD_LINE_ARG_EMIT_LLVM) == 0)
			{
				emitLLVM = true;
			}
			else if (strcmp(arg, CMD_LINE_ARG_OPT_LEVEL_1) == 0)
			{
				optLevel = 1;
			}
			else if (strcmp(arg, CMD_LINE_ARG_OPT_LEVEL_2) == 0)
			{
				optLevel = 2;
			}
			else if (strcmp(arg, CMD_LINE_ARG_OPT_LEVEL_3) == 0)
			{
				optLevel = 3;
			}
			else
			{
				SnekFatal(context, ERROR_CODE_CMD_ARG_SYNTAX, "Undefined command line argument %s", arg);
				result = false;
			}
		}
		else
		{
			char* path = _strdup(argv[i]);
			const char* extension = GetExtensionFromPath(path);
			char* moduleName = GetModuleNameFromPath(path);
			if (strcmp(moduleName, "*") == 0)
			{
				char* folder = GetFolderFromPath(path);
				result = AddSourceFolder(context, folder, extension, false) && result;
				delete folder, moduleName;
			}
			else if (strcmp(moduleName, "**") == 0)
			{
				char* folder = GetFolderFromPath(path);
				result = AddSourceFolder(context, folder, extension, true) && result;
				delete folder, moduleName;
			}
			else
			{
				result = AddFile(context, path) && result;
			}
		}
	}

	if (result)
	{
		bool benchmark = true;

		int64_t parser_before = 0, parser_after = 0;
		int64_t resolver_before = 0, resolver_after = 0;
		int64_t codegen_before = 0, codegen_after = 0;
		int64_t link_before = 0, link_after = 0;

		float parser_duration = 0.0f;
		float resolver_duration = 0.0f;
		float codegen_duration = 0.0f;
		float link_duration = 0.0f;

		parser_before = getNanos();

		DBG_LOG("Parsing... ");
		if (SnekRunParser(context))
		{
			parser_after = getNanos();
			parser_duration = (parser_after - parser_before) / 1e9f;
			resolver_before = parser_after;

			DBG_LOG("Done. %fs\nSemantic analysis... ", parser_duration);
			if (SnekRunResolver(context))
			{
				resolver_after = getNanos();
				resolver_duration = (resolver_after - resolver_before) / 1e9f;
				codegen_before = resolver_after;

				DBG_LOG("Done. %fs\nGenerating code... ", resolver_duration);
				LLVMBackend* cb = CreateLLVMBackend(context);
				if (LLVMBackendCompile(cb, context->asts.buffer, context->asts.size, filename, buildFolder, genDebugInfo, emitLLVM, optLevel))
				{
					codegen_after = getNanos();
					codegen_duration = (codegen_after - codegen_before) / 1e9f;
					link_before = codegen_after;

					DBG_LOG("Done. %fs\nLinking... ", codegen_duration);
					if (LLVMLink(cb, argv[0], filename, genDebugInfo, optLevel, additionalLibPaths))
					{
						link_after = getNanos();
						link_duration = (link_after - link_before) / 1e9f;

						DBG_LOG("Done. %fs\nBuild complete in %fs\n", link_duration, parser_duration + resolver_duration + codegen_duration + link_duration);
					}
					else
					{
						DBG_LOG("Failed!\n");
						result = false;
					}
				}
				else
				{
					DBG_LOG("Failed!\n");
					result = false;
				}

				DestroyLLVMBackend(cb);
			}
			else
			{
				DBG_LOG("Failed!\n");
				result = false;
			}
		}
		else
		{
			DBG_LOG("Failed!\n");
			result = false;
		}
	}

	fprintf(stderr, "%s", compilerMessages.str().c_str());

	DestroySnekContext(context);

	DestroyList(additionalLibPaths);

	return !result;
}
