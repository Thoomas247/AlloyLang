#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
#include "parser/TreePrinter.hpp"
//#include "LLVMCodeGenerator/LLVMCodeGenerator.hpp"
#include "codegen/CodeGenerator.hpp"

#include "log/Log.hpp"
#include "TestString.hpp"

#include <iostream>
#include <fstream>
#include <chrono>

using namespace AlloyCompiler;

int main(int argc, char* argv[])
{
	std::string str;
	if (argc > 1) {
		// read test file provided on the command line
		std::ifstream file(argv[1]);
		if (file) {
			std::string line;
			while (std::getline(file, line)) {
				str += line;
				str += "\r\n";
			}
			file.close();
		}
		else {
			ASSERT(false, "Cannot read input file");
		}
	}
	else {
		str = TestSrc1;
	}

	// duplicate the test file or string 2^n times
	const size_t n = 0;// 0'000;
	if (n > 0) {
		str.reserve(str.size() * pow(2, n));

		for (size_t i = 0; i < n; i++)
		{
			str += str;
		}
	}

	Source source(str);

	const uint64_t numBytes = str.size();
	const uint64_t numKilobytes = std::max(numBytes / 1000, 1ull);

	Log::Print("Compiling {0} kB...", numKilobytes);

	auto start = std::chrono::high_resolution_clock::now();
	TokenBuffers tokenBuffers = Tokenize(source);
	auto end = std::chrono::high_resolution_clock::now();

	const uint64_t tokenizeTime = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 1ll);

	PrintTokens(tokenBuffers);

	start = std::chrono::high_resolution_clock::now();
	auto nodeBuffers = Parse(tokenBuffers);
	end = std::chrono::high_resolution_clock::now();
	const uint64_t parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	PrintTree(tokenBuffers, nodeBuffers, nodeBuffers.GetRootNodeID());

	start = std::chrono::high_resolution_clock::now();
	//LLVMCodeGenerator codegen(tokenBuffers, nodeBuffers);
	//codegen.Process();	// TBD: implement error handling
	Generate(tokenBuffers, nodeBuffers, true);

	end = std::chrono::high_resolution_clock::now();
	const uint64_t codegenTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	Log::Print("Tokenize: {0}ms", tokenizeTime);
	Log::Print("Parse: {0}ms", parseTime);
	AlloyCompiler::Log::Print("Codegen: {0}ms", codegenTime);

	AlloyCompiler::Log::Print("-- Compiled in {0}ms --", tokenizeTime + parseTime + codegenTime);
	AlloyCompiler::Log::Print("Speed: {0}ms/kB", (tokenizeTime + parseTime + codegenTime) / numKilobytes);

	Log::Print("");
}