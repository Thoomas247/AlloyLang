#include "pch.h"
#include "CppUnitTest.h"
#include "../AlloyLang/LLVMCodeGenerator/llvm.hpp"

#include "../AlloyLang/tokenizer/Tokenizer.hpp"
#include "../AlloyLang/parser/Parser.hpp"
#include "../AlloyLang/LLVMCodeGenerator/LLVMCodeGenerator.hpp"

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
	class redirect_stdout : public raw_fd_ostream
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
	public:
		
		TEST_METHOD(IfStatements)
		{
			constexpr auto TestStr = R"(
				extern fn printf (const str : String, const param : i64) -> i64;

				fn mul (const a : &i64, const b : &i64) -> i64
				{
					var ret : i64 = 0;
					if (a == 1)
						ret = b;
					else
						ret = a * b;
					return ret;
				}

				fn main () -> i64
				{
					printf("3 x 5 = %f", mul(3, 5));
					return 0;
				}
			)";
			std::string expected = "3 x 5 = 15.000000";

			Source	src(TestStr);
			TokenBuffers tokenBuffers = Tokenize(src);
			auto nodeBuffers = Parse(tokenBuffers);
			LLVMCodeGenerator codegen(tokenBuffers, nodeBuffers);

			// suppress output to out.ll file
			codegen.Process(&llvm::nulls());

			// interpret and execute resulting code
			codegen.Execute();

			// retrieve result and compare with expected data
			std::string result = static_cast<redirect_stdout &>(llvm::outs()).buffer;
			Assert::AreEqual(expected, result);
		}
	};
}

// replace the default implementation of llvm::outs() in order to redirect stdout to our buffer
// requires linker option /FORCE:MULTIPLE
namespace llvm
{
	raw_fd_ostream& llvm::outs() {
		static unittests::redirect_stdout S;
		return S;
	}
}
