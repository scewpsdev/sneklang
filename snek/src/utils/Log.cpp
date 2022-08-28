#include "log.h"

#include "snek.h"


MessageCallback_t GetMsgCallback(SkContext* context)
{
	return context->msgCallback;
}
