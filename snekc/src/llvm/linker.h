#pragma once

#include "list.h"


struct LLVMBackend;

bool LLVMLink(LLVMBackend* llb, const char* arg0, const char* filename, bool genDebugInfo, int optLevel, List<const char*>& additionalLibPaths);
