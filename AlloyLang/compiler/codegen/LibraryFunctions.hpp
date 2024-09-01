#define StructCompareFunctionName	"_CreateStructCompare_"
#define MemCmpFunctionName	"memcmp"

namespace AlloyCompiler
{
	// generate code to call the standard memcmp library function
	llvm::Function* generateMemCmpFunctionDeclaration(ModuleTable& moduleTable, LLVMState& state);

	// generate code to compare two structures given pointers to these structures
	llvm::Function* generateStructureComparisonFunction(ModuleTable& moduleTable, LLVMState& state);
}
