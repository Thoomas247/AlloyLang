#pragma once
#include "Parser.hpp"
#include "tokenizer/Tokenizer.hpp"

namespace AlloyCompiler::Parser
{
	void PrintTree(const Tokenizer::TokenDataBuffers& tokenBuffers, const NodeDataBuffers& nodeBuffers);
}