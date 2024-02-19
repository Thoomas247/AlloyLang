#pragma once

#ifndef LLVMCODEGENERATOR_INCLUDE
#define LLVMCODEGENERATOR_INCLUDE

#include "../parser/Parser.hpp"
#include "NamedValues.hpp"
#include "../util/SmallStringView.hpp"

#include <memory>
#include <map>
#include <utility>
#include <vector>

using namespace llvm;

namespace AlloyCompiler
{
	class LLVMCodeGenerator
	{
	public:
		LLVMCodeGenerator(const TokenBuffers& tokenBuffers,
			const NodeBuffers& nodeDataBuffers);
		virtual ~LLVMCodeGenerator();

		// generate the LLVM intermediate code (IR)
		int Process(llvm::raw_ostream* llvmOutput = nullptr);
		// execute generated code
		int Execute();

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
		const NodeBuffers& NodeBuffers;
		const TokenBuffers& TokenBuffers;

		// convert escape sequences to their actual values
		std::string UnescapeString(const SmallStringView& str);

		// support for mutable variables
		AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName, llvm::Type* type);

		// recursively process all nodes
		Value* codegen(const Node& node);
		Value* codegen(NodeID nodeID);

		Value* codegen(const LITERAL& node);
		Value* codegen(const BINARY_EXPRESSION& node);
		Value* codegen(const VALUE_DEFINITION& node);
		Value* codegen(const VALUE_DEFINITION_EXPRESSION& node);
		Value* codegen(const IDENTIFIER& node);
		Value* codegen(const MEMBER_ACCESS_EXPRESSION& node, Value** ObjectPtr = nullptr);	// for this specific expression, we need the value and the pointer to the object
		Function* codegen(const FUNCTION_DEFINITION& node);
		Function* codegen(const FUNCTION_DECLARATION& node);
		Value* codegen(const BLOCK_STATEMENT& node);
		Value* codegen(const FUNCTION_CALL_EXPRESSION& node);
		Value* codegen(const IF_STATEMENT& node);
		Value* codegen(const FOR_LOOP_STATEMENT& node);
		Value* codegen(const ASSIGNMENT_EXPRESSION& node);
		Value* codegen(const RETURN_STATEMENT& node);
		Value* codegen(const STRUCT_DEFINITION& node);
		Value* codegen(const CONSTRUCTOR_EXPRESSION& node);

		// straight-forward cases

		// statements should not return a result, only true or false depending on success or failure
		// assignment statement
		Value* codegen(const ASSIGNMENT_STATEMENT& node) { return ConstantInt::getBool(*TheContext, codegen(node.AssignmentExpressionID) != nullptr); }

		// value definition statement
		Value* codegen(const VALUE_DEFINITION_STATEMENT& node) { return ConstantInt::getBool(*TheContext, codegen(node.ValueDefinitionExpressionID) != nullptr); }

		// function call statement
		Value* codegen(const FUNCTION_CALL_STATEMENT& node) { return ConstantInt::getBool(*TheContext, codegen(node.FunctionCallExpressionID) != nullptr); }

		// external function definition
		Function* codegen(const EXTERN_DEFINITION& node) { return static_cast<llvm::Function*>(codegen(node.FunctionDeclarationID)); }

		// (expression)
		Value* codegen(const ENCLOSED_EXPRESSION& node) { return codegen(node.ExpressionID); }

		// helper methods used by the codegen methods
		Function* createFunctionPrototype(const std::string& Name, const FUNCTION_DECLARATION& node);
		bool updateValueOfLocalOrGlobalVariable(const std::string& VariableName, Value* Value);
		Value* loadValueOfLocalOrGlobalVariable(const std::string& VariableName, Value*& Value);

		friend class NamedStructs;
	};
}
#endif		// LLVMCODEGENERATOR_INCLUDE
