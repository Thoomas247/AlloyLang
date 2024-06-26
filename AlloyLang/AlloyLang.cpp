#include "Compiler.hpp"

using namespace AlloyCompiler;

int main(int argc, char* argv[])
{
	ASSERT(argc == 2, "Invalid number of arguments!");

	Compiler compiler(argv[1]);
	compiler.Compile();
}