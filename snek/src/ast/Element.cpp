#include "Element.h"

#include "parser/input.h"


namespace AST
{
	SourceLocation::SourceLocation(const InputState& inputState)
		: filename(inputState.filename), line(inputState.line), col(inputState.col)
	{
	}

	Element::Element(File* file, const SourceLocation& location)
		: file(file), location(location)
	{
	}
}
