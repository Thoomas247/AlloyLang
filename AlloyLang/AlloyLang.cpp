#include "log/Log.hpp"
#include "compiler/Compiler.hpp"

using namespace AlloyCompiler;

int main(int argc, char* argv[])
{
	ASSERT(argc == 2, "Invalid number of arguments!");

	Compiler compiler(argv[1], /*optimize*/true);
	compiler.Compile();
}