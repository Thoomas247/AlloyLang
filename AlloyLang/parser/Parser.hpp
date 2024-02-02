#pragma once
#include "Node.hpp"
#include "../tokenizer/Token.hpp"

namespace AlloyCompiler
{
	NodeBuffers Parse(const TokenBuffers& tokenBuffers);
}