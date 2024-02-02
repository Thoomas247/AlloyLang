#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
#include "parser/TreePrinter.hpp"
#include "LLVMCodeGenerator/LLVMCodeGenerator.hpp"

#include "log/Log.hpp"
#include "TestString.hpp"

#include <chrono>

using namespace AlloyCompiler;

int main()
{
	const size_t n = 0;// 0'000;
	std::string str = "const res : i64 = 1+2;var e : f32 = res + 1.01;"; // TestSrc;
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

	Log::Print("Tokenize: {0}ms", tokenizeTime);

	start = std::chrono::high_resolution_clock::now();
	auto nodeBuffers = Parse(tokenBuffers);
	end = std::chrono::high_resolution_clock::now();
	const uint64_t parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	Log::Print("Parse: {0}ms", parseTime);

	PrintTree(tokenBuffers, nodeBuffers);

	start = std::chrono::high_resolution_clock::now();
	LLVMCodeGenerator* codegen = new LLVMCodeGenerator(tokenBuffers, nodeBuffers);
	codegen->Process();	// TBD: implement error handling
	delete codegen;
	end = std::chrono::high_resolution_clock::now();
	const uint64_t codegenTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
  AlloyCompiler::Log::Print("Codegen: {0}ms", codegenTime);

	AlloyCompiler::Log::Print("-- Compiled in {0}ms --", tokenizeTime + parseTime + codegenTime);
	AlloyCompiler::Log::Print("Speed: {0}ms/kB", (tokenizeTime + parseTime + codegenTime) / numKilobytes);

	Log::Print("");
}