#include "compiler/codegen/llvm/llvm.hpp"
#include "compiler/codegen/NamedValues.hpp"
#include "compiler/codegen/CodeGenerator.hpp"
#include "compiler/codegen/AlloyType.hpp"
#include "compiler/codegen/AlloyValue.hpp"
#include "compiler/codegen/LibraryFunctions.hpp"
#include "compiler/codegen/Inlines.hpp"
#include "compiler/codegen/SmartPointerClass.hpp"
#include "log/Log.hpp"
#include "compiler/Compiler.hpp"

using namespace AlloyCompiler;

int main(int argc, char* argv[])
{
	ASSERT(argc >= 2, "Invalid number of arguments!");

	if (argc > 2)
	{
		// use optimization value from command line
		int optimization = atoi(argv[2]);
		ASSERT(optimization >= 0 && optimization <= 2, "Invalid optimization value!");
		Compiler compiler(argv[1], (LLVMState::LLVMOptimizations)atoi(argv[2]));
		compiler.Compile();
	}
	else
	{
		// use default optimization value
		Compiler compiler(argv[1]);
		compiler.Compile();
	}
}