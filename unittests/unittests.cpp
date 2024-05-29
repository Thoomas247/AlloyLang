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
			Tokenizer tokenizer(src);
			TokenBuffers tokenBuffers = tokenizer.Tokenize();
			Parser parser(src, tokenBuffers);
			NamedNodes namedNodes = parser.Parse();

			LLVMState state(true);
			Generate(namedNodes, state);
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
				extern printf (const str : String) -> i32;

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
				extern printf (const str : String, const param : i64) -> i64;

				fn mul (const a : i64, const b : i64) -> i64
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
				extern printf (const str : String, const param : i64) -> i64;

				fn mul (const a : i64, const b : i64) -> i64
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
				extern printf (const str : String, const param : i64) -> i64;

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
				extern printf (const str : String, const param : f64) -> i64;

				type Vector3 = struct
				{
					x : f32,
					y : f32,
					z : f32
				};

				type Transform = struct
				{
					position : Vector3,
					rotation : Vector3,
					scale : Vector3
				};

				fn translate(const point : Vector3, const vector : Vector3) -> Vector3
				{
					var result : Vector3 = Vector3 { x = 0.00, y = 0.00, z = 0.00 };
					
					result.x = point.x + vector.x;
					result.y = point.y + vector.y;
					result.z = point.z + vector.z;
					
					return result;
				}

				fn main () -> i64
				{
					var a : f32 = 32.0;
					var expected : f32 = 20480.0;
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

					const translation : Vector3 = Vector3 { x = 0, y = 0, z = 10 };

					test.z = translate(test, translation).z;
					test2.rotation.y = 20.0;

					printf("result = %f", test.x * test2.scale.y * test.z);
					if (test.x * test2.scale.y * test.z == expected)
						return 1;
					else
						return 0;
				}
			)";
			std::string expected = "result = 20480.000000";

			RunTest(TestStr, 1);
		}
		
		TEST_METHOD(Arrays)
		{
			constexpr auto TestStr = R"(
			extern printf (const str : String, const param : i64) -> i64;

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

		TEST_METHOD(ArraysInStructures)
		{
			constexpr auto TestStr = R"(
				extern printf (const str : String, const param : f64) -> i64;

				type Vector3 = struct
				{
					x : f64,
					y : f64,
					z : f64
				};

				type StructWithArray = struct
				{
					x : i64,
					array : [f64; 2]
				};

				fn translate(const point : Vector3, const vector : Vector3) -> Vector3
				{
					var result : Vector3 = Vector3 { x = 0.00, y = 0.00, z = 0.00 };
					
					result.x = point.x + vector.x;
					result.y = point.y + vector.y;
					result.z = point.z + vector.z;
					
					return result;
				}

				fn main () -> i64
				{
					var test : Vector3 = 
						Vector3 
						{ 
							x = 32.0, 
							y = 64.0, 
							z = 5.0 
						};

					var atest : StructWithArray = 
						StructWithArray
						{
							x = 2,
							array = { 10.0, 20.0 }
						};
					test.z = atest.array[0];
					test.z = translate(test, test).z;

					printf("result = %f", test.z);

					if (test.z == 20.0)
						return 1;
					else
						return 0;
				}
			)";
			std::string expected = "result = 20.0000";
			RunTest(TestStr, 1);
		}

		TEST_METHOD(Pointers)
		{
			constexpr auto TestStr = R"(
				extern printf (const str : String, const param : i64) -> i64;
	
				type TestStruct = struct
				{
					a : i64,
					b : i64
				};
	
				fn main () -> i64
				{				
					// array of i64
					var array : [i64; 10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

					// new keyword allocates value of expression on heap and returns pointer to it
					const a : TestStruct = TestStruct { a = 2, b = 5 };
					var ptr1 : *TestStruct = new a;
					var ptr2 : *[i64] = new [123; 4];
					var ptr3 : *i64 = new 3;
			
					// array of pointers to i64
					const ptrArray : [*i64; 3] = { new array[1], new a.b, new 12 };

					// array of i64 from ptrArray
					const arr : [i64; 3] = { ptrArray[0], ptrArray[1], ptrArray[2] };

					// no dereference needed to perform operations on value being pointed to
					ptr1.a = ptr1.b + 1;
					ptr2[1] = ptr2[1] + 1;

					// change ownership of pointed value to new pointer
					// old ptr is now invalid and cannot be used
					var new_ptr : *i64 = move ptr3;

					printf("ptr1.a = %d\n", ptr1.a);
					printf("ptr2[1] = %d\n", ptr2[1]);
					printf("ptrArray[0] * array[1] * ptrArray[2] * new_ptr = 2 * 5 * 12 * 3 = %d", ptrArray[0] * arr[1] * ptrArray[2] * new_ptr);
					if (ptr1.a == 6 && ptr2[1] == 124 && ptrArray[0] * arr[1] * ptrArray[2]  * new_ptr == 360)
						return 1;
					else
						return 0;
				}
					)";
			std::string expected = "ptr1.a = 6\nptr2[1] = 124\n";

			RunTest(TestStr, 1);
		}

		TEST_METHOD(References)
		{
			constexpr auto TestStr = R"(
				extern printf (const str : String, const param : i64) -> i64;
	
				fn test (var a : i64, var b : &i64) -> i64
				{
					b = a;
					return a;
				}
	
				fn main () -> i64
				{				
					var a : i64 = 3;
					var b : i64 = 0;
					test(a, b);

					// array of references to i64
					var refArray : [&i64; 3] = { &a, &b, &a };

					printf("b = %d", refArray[1]);
					if (b == 3)
						return 1;
					else
						return 0;
				}
			)";
			std::string expected = "b = 3";

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
