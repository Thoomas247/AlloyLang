#include "CppUnitTest.h"
#include "../AlloyLang/compiler/codegen/llvm/llvm.hpp"

#include <iostream>
#include <fstream>
#include <chrono>
#include <io.h>

#include "../AlloyLang/compiler/codegen/NamedValues.hpp"
#include "../AlloyLang/compiler/codegen/CodeGenerator.hpp"
#include "../AlloyLang/compiler/Compiler.hpp"
#include "../AlloyLang/compiler/module/token/TokenBuffer.hpp"
#include "../AlloyLang/compiler/module/node/NodeBuffer.hpp"
#include "../AlloyLang/compiler/codegen/AlloyType.hpp"
#include "../AlloyLang/compiler/codegen/AlloyValue.hpp"
#include "../AlloyLang/compiler/codegen/SmartPointerClass.hpp"

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
			// run the test with all possible optimization options
			const wchar_t* optimizations[] = { L"No optimization", L"Function optimization", L"Module optimization" };
			for (int opt = 1; opt <= 1; opt++) {
				Compiler compiler(path, (LLVMState::LLVMOptimizations) opt);
				compiler.Compile();
				int result = compiler.Execute();
				Assert::AreEqual<int>(expected, result, optimizations[opt]);
			}
		}

	public:

		TEST_METHOD(AnyParameters)
		{
			RunTest("../../AlloyLang/test/AnyParameters/main.alloy", 49);
		}

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

		TEST_METHOD(Enums)
		{
			RunTest("../../AlloyLang/test/Enums/main.alloy", 43);
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

		TEST_METHOD(GenericFunctions)
		{
			RunTest("../../AlloyLang/test/GenericFunctions/main.alloy", 0);
		}

		TEST_METHOD(MultipleFiles)
		{
			RunTest("../../AlloyLang/test/MultipleFiles/main.alloy", 32 * 45);
		}

		TEST_METHOD(MultipleFiles2)
		{
			RunTest("../../AlloyLang/test/MultipleFiles2/main.alloy", 3);
		}

		TEST_METHOD(CallbackFunctions)
		{
			RunTest("../../AlloyLang/test/CallbackFunctions/main.alloy", 1);
		}
		TEST_METHOD(LargeTest)
		{
			RunTest("../../AlloyLang/test/LargeTest/main.alloy", 0);
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
