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
	private:
		void RunTest(const std::string code, const std::string expected) {

			Source	src(code);
			TokenBuffers tokenBuffers = Tokenize(src);
			auto nodeBuffers = Parse(tokenBuffers);
			LLVMCodeGenerator codegen(tokenBuffers, nodeBuffers);

			// suppress output to out.ll file
			codegen.Process(&llvm::nulls());

			// interpret and execute resulting code
			codegen.Execute();

			// retrieve result and compare with expected data
			std::string result = static_cast<redirect_stdout&>(llvm::outs()).buffer;
			Assert::AreEqual(expected, result);
		}

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
					printf("3 x 5 = %d", mul(3, 5));
					return 0;
				}
			)";
			std::string expected = "3 x 5 = 15";
			RunTest(TestStr, expected);
		}

		TEST_METHOD(ForLoop)
		{
			constexpr auto TestStr = R"(
				extern fn printf (const str : String, const param : i64) -> i64;

				fn mul (const a : &i64, const b : &i64) -> i64
				{
					var ret : i64 = 0;
					for (var i : i64 = 0; i < a; i = i + 1) {
						ret = ret + b;
					}
					return ret;
				}

				fn main () -> i64
				{
					printf("3 x 5 = %d", mul(3, 5));
					return 0;
				}
			)";
			std::string expected = "3 x 5 = 15";

			RunTest(TestStr, expected);
		}

		TEST_METHOD(Structures)
		{
			constexpr auto TestStr = R"(
				extern fn printf (const str : String, const param : f32) -> i64;

				struct Vector3
				{
					var x : f32;
					var y : f32;
					var z : f32;
				}

				fn main () -> i64
				{
					var test : Vector3 = 
						Vector3 
						{ 
							x = 32.0, 
							y = 64.0, 
							z = 0.0 
						};
					test.z = 10.0;
					printf("test.z = %f", test.z);
					return 0;
				}
			)";
			std::string expected = "test.z = 10.000000";

			RunTest(TestStr, expected);
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
