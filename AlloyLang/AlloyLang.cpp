#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
//#include "ecs/ECSResolver.hpp"
//#include "codegen/CodeGenerator.hpp"

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

	Source source(str);

	const uint64_t numBytes = str.size();
	const uint64_t numKilobytes = std::max(numBytes / 1000, 1ull);

	Log::Print("Compiling {0} kB...", numKilobytes);

	auto start = std::chrono::high_resolution_clock::now();
	Tokenizer tokenizer(source);
	TokenBuffers tokenBuffers = tokenizer.Tokenize();
	auto end = std::chrono::high_resolution_clock::now();

	const uint64_t tokenizeTime = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 1ll);

	//PrintTokens(tokenBuffers);

	start = std::chrono::high_resolution_clock::now();
	Parser parser(source, tokenBuffers);
	NamedNodes namedNodes = parser.Parse();
	end = std::chrono::high_resolution_clock::now();
	const uint64_t parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	//PrintTree(tokenBuffers, nodeBuffers, nodeBuffers.GetRootNodeID());

	start = std::chrono::high_resolution_clock::now();
	//SystemSchedulingInfo systemSchedulingInfo = ResolveApplication(tokenBuffers, nodeBuffers, nodeBuffers.GetApplicationNodeIDs()[0]);
	//end = std::chrono::high_resolution_clock::now();
	const uint64_t resolutionTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	//systemSchedulingInfo.Print();

	start = std::chrono::high_resolution_clock::now();
	//LLVMState state(true);
	//Generate(tokenBuffers, nodeBuffers, state);
	end = std::chrono::high_resolution_clock::now();
	const uint64_t codegenTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	Log::Print("Tokenize: {0}ms", tokenizeTime);
	Log::Print("Parse: {0}ms", parseTime);
	Log::Print("Resolution: {0}ms", resolutionTime);
	AlloyCompiler::Log::Print("Codegen: {0}ms", codegenTime);

	AlloyCompiler::Log::Print("-- Compiled in {0}ms --", tokenizeTime + parseTime + resolutionTime + codegenTime);
	AlloyCompiler::Log::Print("Speed: {0}ms/kB", (tokenizeTime + parseTime + resolutionTime + codegenTime) / numKilobytes);

	Log::Print("");
}