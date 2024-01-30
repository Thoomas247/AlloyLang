#pragma once
#include "Source.hpp"
#include "Token.hpp"

namespace AlloyCompiler
{
	TokenBuffers Tokenize(const Source& source);
}