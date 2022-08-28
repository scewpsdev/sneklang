#pragma once

#include "Message.h"
#include "Result.h"
#include "ResponseError.h"


struct ResponseMessage : Message
{
	int id = -1;
	Result* result = nullptr;
	ResponseError error;
};
