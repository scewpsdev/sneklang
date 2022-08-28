#pragma once

#include "Position.h"

#include "message/Params.h"
#include "message/Result.h"


struct HoverParams : Params
{
	char textDocument[256] = "";
	Position position;
};

struct HoverResult : Result
{
	char* value = nullptr;
};
