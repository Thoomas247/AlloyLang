#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"

#include "log/Log.hpp"
#include "TestString.hpp"

#include <chrono>

int main()
{
	const size_t n = 1'000;
	std::string str = TestSrc;
	str.reserve(str.size() * (n + 1));

	for (size_t i = 0; i < n; i++)
	{
		str += TestSrc;
	}

	const uint64_t numKilobytes = str.size() / 8 / 1000;

	AlloyCompiler::Log::Print("Compiling {0} kB...", numKilobytes);

	auto start = std::chrono::high_resolution_clock::now();

	auto buffers = AlloyCompiler::Tokenizer::Tokenize(str);
	auto application = AlloyCompiler::Parser::Parse(buffers);

	auto end = std::chrono::high_resolution_clock::now();

	uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	AlloyCompiler::Log::Print("-- Compiled in {0}ms --", time);
	AlloyCompiler::Log::Print("Speed: {0}ms/kB", time / numKilobytes);
}