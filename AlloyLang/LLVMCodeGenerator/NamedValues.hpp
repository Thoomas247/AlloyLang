#pragma once

#include <string>
#include <memory.h>
#include <map>

class llvm::AllocaInst;

namespace AlloyCompiler
{

	class CGNamedValues
	{
	public:

		CGNamedValues(llvm::LLVMContext& llvmContext, std::shared_ptr<CGNamedValues> Parent = nullptr);
		~CGNamedValues();

		llvm::AllocaInst* contains(const std::string& Name, bool checkParents = false);

		void insert(const std::string& Name, llvm::AllocaInst* value) { values[Name] = value; }
		void clear() { values.clear(); }
		std::shared_ptr<CGNamedValues>& getParent() { return parent; }

		static llvm::Type* AlloyToLLVMType(llvm::LLVMContext& llvmContext,
			const AlloyCompiler::NodeBuffers& NodeBuffers,
			const AlloyCompiler::TokenBuffers& TokenBuffers,
			AlloyCompiler::NodeID id);
		static void addType(const std::string& AlloyType, llvm::Type* LLVMType) { alloyToLlvmMap[AlloyType] = LLVMType; }
		static int CompareTypes(const llvm::Type* L, const llvm::Type* R);
		static llvm::Type* MakeCompatible(llvm::Value*& L, llvm::Value*& R);

	private:
		std::shared_ptr<CGNamedValues> parent;
		std::map<std::string, llvm::AllocaInst*> values;

		// map from Alloy types (i32, i64, ..) and dynamically defined types (structures) to LLVM types
		static std::map<std::string, llvm::Type*> alloyToLlvmMap;
	};

}	// end of AlloyCompiler namespace
