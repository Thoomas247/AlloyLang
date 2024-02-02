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

#include "../parser/ASTNodes.hpp"
#include "../parser/Parser.hpp"

using namespace llvm;

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
protected:
	std::string Name;
	std::vector<std::string> Args;

public:
	PrototypeAST(const std::string& Name, std::vector<std::string> Args)
		: Name(Name), Args(std::move(Args)) {}

	const std::string& getName() const { return Name; }
	const std::vector<std::string>& getArgs() const { return Args; }
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {

protected:
	std::unique_ptr<PrototypeAST> Proto;
	AlloyCompiler::Node Body;

public:
	FunctionAST(std::unique_ptr<PrototypeAST> Proto,
		const AlloyCompiler::Node& Body)
		: Proto(std::move(Proto)), Body(Body) {}

	const std::unique_ptr<PrototypeAST>& getPrototype() { return Proto; }
	const AlloyCompiler::Node& getBody() { return Body; }
};

class LLVMCodeGenerator
{
public:
	LLVMCodeGenerator(const AlloyCompiler::Tokenizer::TokenDataBuffers& tokenBuffers, 
		const AlloyCompiler::Parser::NodeDataBuffers& nodeDataBuffers);
	~LLVMCodeGenerator();

	int Process();

	LLVMContext& getContext() { return *TheContext; }
	llvm::Module& getModule() { return *TheModule; }

private:
	std::unique_ptr<LLVMContext> TheContext;
	std::unique_ptr<IRBuilder<>> Builder;
	std::unique_ptr<llvm::Module> TheModule;
	std::map<std::string, AllocaInst*> NamedValues;
	const AlloyCompiler::Parser::NodeDataBuffers& NodeBuffers;
	const AlloyCompiler::Tokenizer::TokenDataBuffers& TokenBuffers;

	// support for mutable variables
	AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName);

	// called when top level expression is encountered
	Value* HandleTopLevelExpression(const AlloyCompiler::Node& node);

	// recursively process all nodes
	Value* codegen(const AlloyCompiler::Node& node);
	Value* codegen(uint32_t nodeID);

	Function* codegen(FunctionAST& function);
	Function* codegen(PrototypeAST& prototype);
	Value* codegen(const AlloyCompiler::IntegerLiteral& node);
	Value* codegen(const AlloyCompiler::FloatLiteral& node);
	Value* codegen(const AlloyCompiler::Binary& node);
	Value* codegen(const AlloyCompiler::VariableDefinition& node);
	Value* codegen(const AlloyCompiler::MemoryAccess& node);
};

