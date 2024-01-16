#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
#include "parser/TreePrinter.hpp"

#include "log/Log.hpp"
#include "TestString.hpp"

#include <chrono>

int main()
{
	const size_t n = 1;// 0'000;
	std::string str = TestSrc;
	str.reserve(str.size() * (n + 1));

	for (size_t i = 0; i < n; i++)
	{
		str += TestSrc;
	}

	const uint64_t numBytes = str.size();
	const uint64_t numKilobytes = std::max(numBytes / 1000, 1ull);

	AlloyCompiler::Log::Print("Compiling {0} kB...", numKilobytes);

	auto start = std::chrono::high_resolution_clock::now();
	auto buffers = AlloyCompiler::Tokenizer::Tokenize(str);
	auto end = std::chrono::high_resolution_clock::now();

	const uint64_t tokenizeTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	start = std::chrono::high_resolution_clock::now();
	AlloyCompiler::Parser::NodeDataBuffers nodeBuffers = AlloyCompiler::Parser::Parse(buffers);
	end = std::chrono::high_resolution_clock::now();

	const uint64_t parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	AlloyCompiler::Log::Print("-- Compiled in {0}ms --", tokenizeTime + parseTime);
	AlloyCompiler::Log::Print("Tokenize: {0}ms", tokenizeTime);
	AlloyCompiler::Log::Print("Parse: {0}ms", parseTime);
	AlloyCompiler::Log::Print("Speed: {0}ms/kB", (tokenizeTime + parseTime) / numKilobytes);

	AlloyCompiler::Log::Print("");
}