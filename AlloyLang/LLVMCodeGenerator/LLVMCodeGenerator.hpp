#pragma once

#include <memory>
#include <map>

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include "../parser/Parser.hpp"

using namespace llvm;

class LLVMCodeGenerator
{
public:
	LLVMCodeGenerator(const AlloyCompiler::TokenBuffers& tokenBuffers,
		const AlloyCompiler::NodeBuffers& nodeDataBuffers);
	~LLVMCodeGenerator();

	int Process();

	LLVMContext& getContext() { return *TheContext; }
	llvm::Module& getModule() { return *TheModule; }

private:
	std::unique_ptr<LLVMContext> TheContext;
	std::unique_ptr<IRBuilder<>> Builder;
	std::unique_ptr<llvm::Module> TheModule;
	std::map<std::string, AllocaInst*> NamedValues;
	const AlloyCompiler::NodeBuffers& NodeBuffers;
	const AlloyCompiler::TokenBuffers& TokenBuffers;

	// support for mutable variables
	AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName);

	// recursively process all nodes
	Value* codegen(const AlloyCompiler::Node& node);
	Value* codegen(AlloyCompiler::NodeID nodeID);

	Value* codegen(const AlloyCompiler::LITERAL& node);
	Value* codegen(const AlloyCompiler::BINARY_EXPRESSION& node);
	Value* codegen(const AlloyCompiler::VALUE_DEFINITION& node);
	Value* codegen(const AlloyCompiler::VALUE_DEFINITION_EXPRESSION& node);
	Value* codegen(const AlloyCompiler::IDENTIFIER& node);
	Function* codegen(const AlloyCompiler::FUNCTION_DEFINITION& node);
	Value* codegen(const AlloyCompiler::BLOCK_STATEMENT& node);
	Value* codegen(const AlloyCompiler::FUNCTION_CALL_EXPRESSION& node);

	// straight-forward cases

	// return expression;
	Value* codegen(const AlloyCompiler::RETURN_STATEMENT& node) { return codegen(node.ExpressionID); }

	// (expression)
	Value* codegen(const AlloyCompiler::ENCLOSED_EXPRESSION& node) { return codegen(node.ExpressionID); }


	// helper methods used by the codegen methods
	Function* createFunctionPrototype(const std::string& Name, const AlloyCompiler::FUNCTION_DEFINITION& node);

};
