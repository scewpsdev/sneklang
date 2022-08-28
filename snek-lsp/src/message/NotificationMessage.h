#pragma once

#include "Message.h"
#include "Params.h"


struct NotificationMessage : Message
{
	char method[32] = "";
	Params* params = nullptr;
};
