#pragma once

struct InputState;

typedef void* ValueHandle;
typedef void* ControlFlowHandle;
typedef void* TypeHandle;

namespace AST
{
	struct File;

	struct SourceLocation
	{
		const char* filename = nullptr;
		int line = 0, col = 0;


		SourceLocation(const InputState& inputState);
	};

	struct Element
	{
		File* file = nullptr;
		SourceLocation location;


		Element(File* file, const SourceLocation& location);

		virtual Element* copy() = 0;
	};

	enum class Visibility
	{
		Null = 0,

		Private,
		Public,
		Internal,
		Export,
	};
}
