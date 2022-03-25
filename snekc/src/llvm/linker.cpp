#pragma warning( disable : 4244 )

#include "linker.h"

#include "snek.h"
#include "llvm_backend.h"

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

#include <lld/Common/Driver.h>


static void FindMSVCLibraryPaths(std::vector<const char*>& args) {
	std::filesystem::path windowsKits;
	if (std::filesystem::exists("C:\\Program Files (x86)\\Windows Kits\\10\\Lib")) {
		windowsKits = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib";
	}
	if (!windowsKits.empty()) {
		std::filesystem::path kitsDir;
		for (auto& folder : std::filesystem::directory_iterator(windowsKits)) {
			kitsDir = folder;
		}
		if (!kitsDir.empty()) {
			std::filesystem::path ucrtPath = kitsDir;
			ucrtPath.append("ucrt\\x64");
			std::filesystem::path umPath = kitsDir;
			umPath.append("um\\x64");

			std::string ucrtPathStr = "/libpath:" + ucrtPath.string();
			args.push_back(_strdup(ucrtPathStr.c_str()));

			std::string umPathStr = "/libpath:" + umPath.string();
			args.push_back(_strdup(umPathStr.c_str()));

			//args.push_back(_strdup((const char*)ucrtPath.u8string().c_str()));
			//args.push_back(_strdup((const char*)umPath.u8string().c_str()));
			//paths.push_back(ucrtPath);
			//paths.push_back(umPath);
			//libFiles.push_back("ucrt.lib");
		}
	}

	std::filesystem::path vc = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC";
	std::filesystem::path toolsDir;
	for (auto& folder : std::filesystem::directory_iterator(vc)) {
		toolsDir = folder;
	}
	if (!toolsDir.empty()) {
		std::filesystem::path msvcTools = toolsDir;
		msvcTools.append("lib\\x64");

		std::string msvcToolsStr = "/libpath:" + msvcTools.string();
		args.push_back(_strdup(msvcToolsStr.c_str()));
		//paths.push_back(msvcTools);
		//libFiles.push_back("msvcrt.lib");
		//libFiles.push_back("vcruntime.lib");
	}
}

bool LLVMLink(LLVMBackend* llb, const char* arg0, const char* filename, bool genDebugInfo, int optLevel, List<const char*>& additionalLibPaths)
{
	std::vector<const char*> args;

	args.push_back(arg0);

	std::string outArg = "-out:" + std::string(filename);
	args.push_back(outArg.c_str());

	if (genDebugInfo)
	{
		args.push_back("-debug");
	}
	if (optLevel > 0)
	{
		std::string optArg = std::string("-opt:") + std::to_string(optLevel);
		args.push_back(_strdup(optArg.c_str()));
	}

	//args.push_back("-subsystem:console");
	//args.push_back("-nologo");
	//args.push_back("-entry:Main"); // This makes calling crt functions segfault for some reason

	args.push_back("-defaultlib:libcmt");
	//args.push_back("-defaultlib:oldnames");

	FindMSVCLibraryPaths(args);

	//args.push_back("-libpath:C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.29.30133\\lib\\x64");
	//args.push_back("-libpath:C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.29.30133\\atlmfc\\lib\\x64");
	//args.push_back("-libpath:C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.18362.0\\ucrt\\x64");
	//args.push_back("-libpath:C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.18362.0\\um\\x64");
	//args.push_back("-libpath:C:\\Program Files\\LLVM\\lib\\clang\\12.0.1\\lib\\windows");

	for (int i = 0; i < additionalLibPaths.size; i++)
	{
		std::string argStr = "-libpath:" + std::string(additionalLibPaths[i]);
		args.push_back(_strdup(argStr.c_str())); // TODO clean up string
	}

	for (int i = 0; i < llb->context->linkerFiles.size; i++)
	{
		args.push_back(llb->context->linkerFiles[i].path);
	}

	std::string out;
	std::string err;
	llvm::raw_string_ostream outStream(out);
	llvm::raw_string_ostream errStream(err);
	if (lld::coff::link(args, false, outStream, errStream))
	{
		return true;
	}
	else
	{
		std::cerr << "lld ";
		for (size_t i = 0; i < args.size(); i++)
		{
			std::cerr << args[i] << ' ';
		}
		std::cerr << std::endl;

		std::cerr << err;
		return false;
	}
}
