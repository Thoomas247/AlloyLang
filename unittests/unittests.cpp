#include "CppUnitTest.h"
#include "../AlloyLang/codegen/llvm/llvm.hpp"

#include "../AlloyLang/tokenizer/Tokenizer.hpp"
#include "../AlloyLang/parser/Parser.hpp"
#include "../AlloyLang/codegen/CodeGenerator.hpp"

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
		void RunTest(const std::string code, int expected) {

			Source	src(code);
			TokenBuffers tokenBuffers = Tokenize(src);
			auto nodeBuffers = Parse(tokenBuffers);

			LLVMState state(true);
			Generate(tokenBuffers, nodeBuffers, state);
			int result = Execute(state);

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
			constexpr auto TestStr = R"(
				extern fn printf (const str : String) -> i32;

				fn main () -> i64
				{
					printf("Basic");
					return 1;
				}
			)";
			std::string expected_ = "Basic";
			RunTest(TestStr, 1);
		}

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
					return mul(3, 5);
				}
			)";
			std::string expected = "3 x 5 = 15";
			RunTest(TestStr, 15);
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
					return mul(3, 5);
				}
			)";
			std::string expected = "3 x 5 = 15";

			RunTest(TestStr, 15);
		}

		TEST_METHOD(WhileLoop)
		{
			constexpr auto TestStr = R"(
				extern fn printf (const str : String, const param : i64) -> i64;

				fn main () -> i64
				{
					var a : i64 = 4;
					var i : i64 = 0;
					var res : i64 = 2;
					while (i < a)
					{
						res = res * res;
						i = i + 1;
					}
					printf("2 ^ 16 = %d", res);
					return res;
				}
	
			)";
			std::string expected = "2 ^ 16 = 65536";

			RunTest(TestStr, 65536);
		}

		TEST_METHOD(Structures)
		{
			constexpr auto TestStr = R"(
				extern fn printf (const str : String, const param : f64) -> i64;

				struct Vector3
				{
					var x : f32;
					var y : f32;
					var z : f32;
				}

				struct Transform
				{
					var position : Vector3;
					var rotation : Vector3;
					var scale : Vector3;
				}

				fn main () -> i64
				{
					var a : f32 = 32.0;
					var expected : f32 = 2048.0;
					var result : i64 = 0;
					var test : Vector3 = 
						Vector3 
						{ 
							x = 32.0, 
							y = 64.0, 
							z = 0.0 
						};

					var test2 : Transform = 
						Transform
						{
							position = Vector3 { x = 10.0, y = 5.0, z = 7.0 },
							rotation = Vector3 { x = 0.0, y = 45.0, z = 0.0 },
							scale = test
						};

					test.z = 10.0;
					test2.rotation.y = 20.0;

					printf("result = %f", test.x * test2.scale.y);
					if (test.x * test2.scale.y == expected)
						return 1;
					else
						return 0;
				}
			)";
			std::string expected = "result = 2048.000000";

			RunTest(TestStr, 1);
		}
		
		TEST_METHOD(Arrays)
		{
			constexpr auto TestStr = R"(
			extern fn printf (const str : String, const param : i64) -> i64;

				fn main () -> i64
				{
					var array : [i64; 10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
					var expected : i64 = 1*2*3*4*5*6*7*8*9*10;
					var result : i64 = 1;

					array[2] = 3;

					for (var i : i32 = 0; i < 10; i = i + 1) {
							result = result * array[i];
						}

					printf("result = %d", result);
					if (result == expected)
						result = 1;
					else
						result = 0;
					return result;
				}
			)";
			std::string expected = "result = 3628800";

			RunTest(TestStr, 1);
		}

		TEST_METHOD(Pointers)
		{
			constexpr auto TestStr = R"(
				extern fn printf (const str : String, const param : i64) -> i64;
	
				struct TestStruct
				{
					var a : i64;
					var b : i64;
				}
	
	
				fn main () -> i64
				{
					const a : TestStruct = TestStruct { a = 2, b = 5 };

					// new keyword allocates value of expression on heap and returns pointer to it
					var ptr1 : *TestStruct = new a;
					var ptr2 : *[i64] = new [123; 4];
		
					// no dereference needed to perform operations on value being pointed to
					ptr1.a = ptr1.b + 1;
					ptr2[1] = ptr2[1] + 1;

					printf("ptr1.a = %d\n", ptr1.a);
					printf("ptr2[1] = %d\n", ptr2[1]);
					if (ptr1.a == 6 && ptr2[1] == 124)
						return 1;
					else
						return 0;
				}
			)";
			std::string expected = "ptr1.a = 6\nptr2[1] = 124\n";

			RunTest(TestStr, 1);
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
