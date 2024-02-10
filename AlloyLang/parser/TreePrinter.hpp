#pragma once
#include "Parser.hpp"
#include "../tokenizer/Tokenizer.hpp"

namespace AlloyCompiler
{
	void PrintTree(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID rootNode);
}