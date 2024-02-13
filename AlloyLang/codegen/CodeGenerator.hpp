#pragma once
#include "../parser/Node.hpp"

namespace llvm
{
	class Value;
}

namespace AlloyCompiler
{
	llvm::Value* Generate(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, bool optimize);
}