#include "CppUnitTest.h"
#include "../AlloyLang/compiler/codegen/llvm/llvm.hpp"

#include "../AlloyLang/compiler/Compiler.hpp"
#include "../AlloyLang/compiler/module/token/TokenBuffer.hpp"
#include "../AlloyLang/compiler/module/node/NodeBuffer.hpp"
#include "../AlloyLang/compiler/codegen/CodeGenerator.hpp"

#include <iostream>
#include <fstream>
#include <chrono>
#include <io.h>

using namespace AlloyCompiler;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace unittests
{
	// helper class to redirect stdout to our internal buffer
	// see below the implementation of llvm::out()
	class redirect_stdout : public llvm::raw_fd_ostream
	{
	private:
		std::error_code EC;

	public:
		redirect_stdout() : raw_fd_ostream("-", EC)
		{
			SetBuffer(buffer, sizeof(buffer));
		}

		char	buffer[1024];
	};

	TEST_CLASS(LLVMCodeGeneratorUnitTests)
	{
	private:
		void RunTest(const std::string& path, int expected)
		{
			Compiler compiler(path, /*optimize*/true);
			compiler.Compile();
			int result = compiler.Execute();

#if 0
			// retrieve result and compare with expected data
			// this only works for the interpreter and not for JIT which we are using for unit testing
			std::string result = static_cast<redirect_stdout&>(llvm::outs()).buffer;
			llvm::outs().flush();
#endif

			Assert::AreEqual(expected, result);
		}

	public:

		TEST_METHOD(BasicTest)
		{
			RunTest("../../AlloyLang/test/Basic/main.alloy", 1);
		}

		TEST_METHOD(IfStatements)
		{
			RunTest("../../AlloyLang/test/IfStatements/main.alloy", 15);
		}

		TEST_METHOD(ForLoops)
		{
			RunTest("../../AlloyLang/test/ForLoops/main.alloy", 15);
		}

		TEST_METHOD(WhileLoops)
		{
			RunTest("../../AlloyLang/test/WhileLoops/main.alloy", 65536);
		}

		TEST_METHOD(SwitchCases)
		{
			RunTest("../../AlloyLang/test/SwitchCases/main.alloy", 1);
		}

		TEST_METHOD(Structs)
		{
			RunTest("../../AlloyLang/test/Structs/main.alloy", 1);
		}

		TEST_METHOD(Arrays)
		{
			RunTest("../../AlloyLang/test/Arrays/main.alloy", 1);
		}

		TEST_METHOD(ArraysInStructs)
		{
			RunTest("../../AlloyLang/test/ArraysInStructs/main.alloy", 1);
		}

		TEST_METHOD(Pointers)
		{
			RunTest("../../AlloyLang/test/Pointers/main.alloy", 1);
		}

		TEST_METHOD(References)
		{
			RunTest("../../AlloyLang/test/References/main.alloy", 1);
		}

		TEST_METHOD(MemberFunctions)
		{
			RunTest("../../AlloyLang/test/MemberFunctions/main.alloy", 1);
		}

		TEST_METHOD(Generics)
		{
			RunTest("../../AlloyLang/test/Generics/main.alloy", 1);
		}

		TEST_METHOD(MultipleFiles)
		{
			RunTest("../../AlloyLang/test/MultipleFiles/main.alloy", 32 * 43);
		}
	};
}

#if 0
// this is not needed as we are using the JIT instead of the interpreter for testing
// replace the default implementation of llvm::outs() in order to redirect stdout to our buffer
// requires linker option /FORCE:MULTIPLE
namespace llvm
{
	raw_fd_ostream& llvm::outs() {
		static unittests::redirect_stdout S;
		return S;
	}
}
#endif
