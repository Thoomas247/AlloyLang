#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
#include "parser/TreePrinter.hpp"

#include "log/Log.hpp"
#include "TestString.hpp"

#include <chrono>

using namespace AlloyCompiler;

int main()
{
	const size_t n = 0;// 0'000;
	std::string str = TestSrc;
	str.reserve(str.size() * (n + 1));

	for (size_t i = 0; i < n; i++)
	{
		str += TestSrc;
	}

	Source source(str);

	const uint64_t numBytes = str.size();
	const uint64_t numKilobytes = std::max(numBytes / 1000, 1ull);

	Log::Print("Compiling {0} kB...", numKilobytes);

	auto start = std::chrono::high_resolution_clock::now();
	TokenBuffers tokenBuffers = Tokenize(source);
	//PrintTokens(tokenBuffers);
	auto end = std::chrono::high_resolution_clock::now();

	const uint64_t tokenizeTime = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 1ll);

	Log::Print("Tokenize: {0}ms ({1}kB/ms)", tokenizeTime, numKilobytes / tokenizeTime);

	start = std::chrono::high_resolution_clock::now();
	auto nodeBuffers = Parse(tokenBuffers);
	end = std::chrono::high_resolution_clock::now();

	const uint64_t parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	Log::Print("Parse: {0}ms", parseTime);

	PrintTree(tokenBuffers, nodeBuffers);

	Log::Print("-- Compiled in {0}ms --", tokenizeTime + parseTime);

	Log::Print("");
}