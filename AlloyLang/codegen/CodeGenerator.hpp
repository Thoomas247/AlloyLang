#pragma once
#include "../parser/Node.hpp"

namespace llvm
{
	class Value;
}

namespace AlloyCompiler
{
	bool Generate(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, bool optimize);
}