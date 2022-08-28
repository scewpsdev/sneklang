#pragma once

#include "Message.h"
#include "Params.h"


struct RequestMessage : Message
{
	int id = -1;
	char method[32] = "";
	Params* params = nullptr;
};
