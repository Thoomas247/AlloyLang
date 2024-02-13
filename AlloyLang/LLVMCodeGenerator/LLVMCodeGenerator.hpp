#pragma once

#ifndef LLVMCODEGENERATOR_INCLUDE
#define LLVMCODEGENERATOR_INCLUDE

#include "../parser/Parser.hpp"
#include "NamedValues.hpp"

#include <memory>
#include <map>
#include <utility>
#include <vector>

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
	std::unique_ptr<CGNamedValues> RootNamedValues;
	std::shared_ptr<CGNamedValues> NamedValues;

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
	Function* codegen(const AlloyCompiler::FUNCTION_DECLARATION& node);
	Value* codegen(const AlloyCompiler::BLOCK_STATEMENT& node);
	Value* codegen(const AlloyCompiler::FUNCTION_CALL_EXPRESSION& node);
	Value* codegen(const AlloyCompiler::IF_STATEMENT& node);
	Value* codegen(const AlloyCompiler::FOR_LOOP_STATEMENT& node);
	Value* codegen(const AlloyCompiler::ASSIGNMENT_EXPRESSION& node);
	Value* codegen(const AlloyCompiler::RETURN_STATEMENT& node);

	// straight-forward cases

	// assignment statement
	// statements should not return a result
	Value* codegen(const AlloyCompiler::ASSIGNMENT_STATEMENT& node) { codegen(node.AssignmentExpressionID); return nullptr; }

	// value definition statement
	// statements should not return a result
	Value* codegen(const AlloyCompiler::VALUE_DEFINITION_STATEMENT& node) { codegen(node.ValueDefinitionExpressionID); return nullptr; }

	// external function definition
	Function* codegen(const AlloyCompiler::EXTERN_DEFINITION& node) { return static_cast<llvm::Function *>(codegen(node.FunctionDeclarationID)); }
		
	// function call statement
	// statements should not return a result
	Value* codegen(const AlloyCompiler::FUNCTION_CALL_STATEMENT& node) { codegen(node.FunctionCallExpressionID); return nullptr; }

	// (expression)
	Value* codegen(const AlloyCompiler::ENCLOSED_EXPRESSION& node) { return codegen(node.ExpressionID); }

	// helper methods used by the codegen methods
	Function* createFunctionPrototype(const std::string& Name, const AlloyCompiler::FUNCTION_DECLARATION& node);
	bool updateValueOfLocalOrGlobalVariable(const std::string& Name, Value* Value);
	llvm::Type* AlloyToLLVMType(AlloyCompiler::NodeID id);
};

#endif		// LLVMCODEGENERATOR_INCLUDE
