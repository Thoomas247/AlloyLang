#pragma once
#include "tokenizer/Tokenizer.hpp"

#include "ASTNode.hpp"

namespace AlloyCompiler::Parser
{
	Ptr<Program> Parse(const Tokenizer::TokenDataBuffers& buffers);
}