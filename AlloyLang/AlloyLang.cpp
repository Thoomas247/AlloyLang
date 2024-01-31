#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
#include "parser/TreePrinter.hpp"
#include "LLVMCodeGenerator/LLVMCodeGenerator.hpp"

#include "log/Log.hpp"
#include "TestString.hpp"

#include <chrono>

int main()
{
	const size_t n = 0;// 0'000;
	std::string str = "const res : i64 = 1+2;"; // TestSrc;
	str.reserve(str.size() * (n + 1));

	for (size_t i = 0; i < n; i++)
	{
		str += TestSrc;
	}

	const uint64_t numBytes = str.size();
	const uint64_t numKilobytes = std::max(numBytes / 1000, 1ull);

	AlloyCompiler::Log::Print("Compiling {0} kB...", numKilobytes);

	auto start = std::chrono::high_resolution_clock::now();
	auto tokenBuffers = AlloyCompiler::Tokenizer::Tokenize(str);
	auto end = std::chrono::high_resolution_clock::now();

	const uint64_t tokenizeTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	start = std::chrono::high_resolution_clock::now();
	auto nodeBuffers = AlloyCompiler::Parser::Parse(tokenBuffers);
	end = std::chrono::high_resolution_clock::now();
	const uint64_t parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	AlloyCompiler::Parser::PrintTree(tokenBuffers, nodeBuffers);

	start = std::chrono::high_resolution_clock::now();
	LLVMCodeGenerator* codegen = new LLVMCodeGenerator(nodeBuffers);
	codegen->Process();	// TBD: implement error handling
	delete codegen;
	end = std::chrono::high_resolution_clock::now();
	const uint64_t codegenTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	AlloyCompiler::Log::Print("-- Compiled in {0}ms --", tokenizeTime + parseTime);
	AlloyCompiler::Log::Print("Tokenize: {0}ms", tokenizeTime);
	AlloyCompiler::Log::Print("Parse: {0}ms", parseTime);
	AlloyCompiler::Log::Print("Codegen: {0}ms", codegenTime);
	AlloyCompiler::Log::Print("Speed: {0}ms/kB", (tokenizeTime + parseTime + codegenTime) / numKilobytes);

	AlloyCompiler::Log::Print("");
}