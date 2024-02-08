#pragma once

#ifndef LLVMCODEGENERATOR_INCLUDE
#define LLVMCODEGENERATOR_INCLUDE

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#ifndef NO_CODE_OPTIMIZATION
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils.h"
#endif // NO_CODE_OPTIMIZATION

#include "../parser/Parser.hpp"

#include <memory>
#include <map>
#include <utility>

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

#ifndef NO_CODE_OPTIMIZATION
	// handling llvm copde optimizations passes
	std::unique_ptr<FunctionPassManager> TheFPM;
	std::unique_ptr<LoopAnalysisManager> TheLAM;
	std::unique_ptr<FunctionAnalysisManager> TheFAM;
	std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
	std::unique_ptr<ModuleAnalysisManager> TheMAM;
	std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
	std::unique_ptr<StandardInstrumentations> TheSI;
#endif // NO_CODE_OPTIMIZATION


	// buffers returned by lexer and parser
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
	Value* codegen(const AlloyCompiler::IF_STATEMENT& node);

	// straight-forward cases

	// return expression;
	Value* codegen(const AlloyCompiler::RETURN_STATEMENT& node) { return codegen(node.ExpressionID); }

	// (expression)
	Value* codegen(const AlloyCompiler::ENCLOSED_EXPRESSION& node) { return codegen(node.ExpressionID); }


	// helper methods used by the codegen methods
	Function* createFunctionPrototype(const std::string& Name, const AlloyCompiler::FUNCTION_DEFINITION& node);

};

#endif		// LLVMCODEGENERATOR_INCLUDE
