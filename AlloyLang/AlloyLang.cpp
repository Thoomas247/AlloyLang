#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
#include "macro/MacroEngine.hpp"
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

	start = std::chrono::high_resolution_clock::now();
	Parser parser(source, tokenBuffers);
	NamedNodes namedNodes = parser.Parse();
	end = std::chrono::high_resolution_clock::now();
	const uint64_t parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	// MACRO TEST
	MacroEngine macroEngine(namedNodes);

	for (auto& [macroName, macroDefinition] : namedNodes.MacroDefinitions)
	{
		// name of the macro we want to execute
		Token macroNameToken = Token{ .Value = macroName, .Location = Location(1, 1, 1), .Kind = TokenKind::identifier };

		// name of the argument(s) we are passing to the macro
		Token typeNameToken = Token{ .Value = "Entity", .Location = Location(1, 1, 1), .Kind = TokenKind::identifier};
		MACRO_VARIABLE_IDENTIFIER typeNameNode = MACRO_VARIABLE_IDENTIFIER{ .pNameToken = &typeNameToken };

		// set up the macro call we want the macro engine to execute
		MACRO_CALL macroCall = MACRO_CALL { .pMacroNameToken = &macroNameToken, .Arguments = { &typeNameNode } };

		// the result is an std::optional<MacroVariable>
		// if has_value() is true, the operation was a success and either a TYPE or FUNCTION_DEFINITION is stored in MacroVariable
		// if has_value() is false, some error occured when trying to evaluate the macro
		MacroResult result = macroEngine.RunMacro(macroCall);
	}

	start = std::chrono::high_resolution_clock::now();
	//LLVMState state(true);
	//Generate(tokenBuffers, nodeBuffers, state);
	end = std::chrono::high_resolution_clock::now();
	const uint64_t codegenTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	Log::Print("Tokenize: {0}ms", tokenizeTime);
	Log::Print("Parse: {0}ms", parseTime);
	AlloyCompiler::Log::Print("Codegen: {0}ms", codegenTime);

	AlloyCompiler::Log::Print("-- Compiled in {0}ms --", tokenizeTime + parseTime + codegenTime);
	AlloyCompiler::Log::Print("Speed: {0}ms/kB", (tokenizeTime + parseTime + codegenTime) / numKilobytes);

	Log::Print("");
}