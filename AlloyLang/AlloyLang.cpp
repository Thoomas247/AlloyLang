#include "tokenizer/Tokenizer.hpp"
#include "parser/ParserCompact.hpp"

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
	auto end = std::chrono::high_resolution_clock::now();

	const uint64_t tokenizeTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	start = std::chrono::high_resolution_clock::now();
	(void)AlloyCompiler::ParserCompact::Parse(buffers);
	end = std::chrono::high_resolution_clock::now();

	const uint64_t parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	AlloyCompiler::Log::Print("-- Compiled in {0}ms --", tokenizeTime + parseTime);
	AlloyCompiler::Log::Print("Tokenize: {0}ms", tokenizeTime);
	AlloyCompiler::Log::Print("Parse: {0}ms", parseTime);
	AlloyCompiler::Log::Print("Speed: {0}ms/kB", (tokenizeTime + parseTime) / numKilobytes);

	// unique_ptr implementation times:
	// 
	// TestSrc.size() * 1'000 bytes:
	// tokenize: 9ms
	// parse: 500ms
	// 
	// TestSrc.size() * 10'000 bytes
	// tokenize: 91ms
	// parse: 50000ms

	// optimized unique_ptr implementation times:
	// 
	// TestSrc.size() * 1'000 bytes:
	// tokenize: 9ms
	// parse: 6ms
	// 
	// TestSrc.size() * 10'000 bytes
	// tokenize: 91ms
	// parse: 69ms

	// flat array implementation times:
	// 
	// TestSrc.size() * 1'000 bytes:
	// tokenize: 9ms
	// parse: 4ms
	// 
	// TestSrc.size() * 10'000 bytes
	// tokenize: 95ms
	// parse: 45ms


}