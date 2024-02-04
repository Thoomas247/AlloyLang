#include <vector>
#include "LLVMCodeGenerator.hpp"

using namespace llvm;
using namespace AlloyCompiler;

LLVMCodeGenerator::LLVMCodeGenerator(const AlloyCompiler::TokenBuffers& tokenBuffers,
	const AlloyCompiler::NodeBuffers& nodeBuffers)
	: NodeBuffers(nodeBuffers), TokenBuffers(tokenBuffers) {

	// Open a new context and module.
	TheContext = std::make_unique<LLVMContext>();
	TheModule = std::make_unique<llvm::Module>("AlloyLang", *TheContext);

	// Create a new builder for the module.
	Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

LLVMCodeGenerator::~LLVMCodeGenerator() {

}

int LLVMCodeGenerator::Process() {
    int result = 0;
    NodeID root = NodeBuffers.GetRootNodeID();
    if (ERROR_NODE_ID == root) {
        // TBD: implement error handling
        assert(false);
        return -1;
    }

	const Node& rootNode = NodeBuffers.GetNode(root);

	result = (codegen(rootNode) ? 0 : -1);
	TheModule->print(errs(), nullptr);
	return result;
}

// called when top level expression is encountered
Value* LLVMCodeGenerator::HandleTopLevelExpression(const AlloyCompiler::Node& node) {
	std::unique_ptr<PrototypeAST> Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
	std::unique_ptr<FunctionAST> Function = std::make_unique<FunctionAST>(std::move(Proto), node);

	// Evaluate a top-level expression into an anonymous function.
	if (auto* FnIR = codegen(*Function)) {
		FnIR->print(errs());
		fprintf(stderr, "\n");

		// Remove the anonymous expression.
		FnIR->eraseFromParent();
	}

	return nullptr;
}

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
/// TBD: only works for doubles, need to implement other types
AllocaInst* LLVMCodeGenerator::CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName) {
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
		TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr,
		VarName);
}

Function* LLVMCodeGenerator::codegen(PrototypeAST& prototype) {

	// Make the function type:  double(double,double) etc.
	std::vector<Type*> Doubles(prototype.getArgs().size(), Type::getDoubleTy(*TheContext));
	FunctionType* FT =
		FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);

	Function* F =
		Function::Create(FT, Function::ExternalLinkage, prototype.getName(), *TheModule);

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto& Arg : F->args())
		Arg.setName(prototype.getArgs()[Idx++]);

	return F;
}

Function* LLVMCodeGenerator::codegen(FunctionAST& function) {

	// First, check for an existing function from a previous 'extern' declaration.
	Function* F = TheModule->getFunction(function.getPrototype()->getName());

	if (!F)
		F = codegen(*function.getPrototype());

	if (!F)
		return nullptr;

	// Create a new basic block to start insertion into.
	BasicBlock* BB = BasicBlock::Create(*TheContext, "entry", F);
	Builder->SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto& Arg : F->args()) {
		// Create an alloca for this variable.
		AllocaInst* Alloca = CreateEntryBlockAlloca(F, std::string(Arg.getName()));

		// Store the initial value into the alloca.
		Builder->CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues[std::string(Arg.getName())] = Alloca;
	}

	if (Value* RetVal = codegen(function.getBody())) {
		// Finish off the function.
		Builder->CreateRet(RetVal);

		// Validate the generated code, checking for consistency.
		verifyFunction(*F);

		return F;
	}


	// Error reading body, remove function.
	F->eraseFromParent();
	return nullptr;
}

Value* LLVMCodeGenerator::codegen(const Node& node) {

	Value* result = nullptr;

	switch (node.Kind) {
	case NodeKind::PROGRAM:
	{
		std::vector<NodeID>::const_iterator iter = node.Program.ModuleIDs.begin();
		for (iter; iter < node.Program.ModuleIDs.end(); iter++) {
			codegen(*iter);
		}
		break;
	}

	case NodeKind::MODULE:
	{
		std::vector<NodeID>::const_iterator iter = node.Module.QualifiedDefinitionIDs.begin();
		for (iter; iter < node.Module.QualifiedDefinitionIDs.end(); iter++) {
			codegen(*iter);
		}
		break;
	}

	case NodeKind::QUALIFIED_DEFINITION:
		codegen(node.QualifiedDefinition.DefinitionID);
		break;

	case NodeKind::VALUE_DEFINITION:
		codegen(node.ValueDefinition);
		break;

	case NodeKind::VALUE_DEFINITION_EXPRESSION:
		codegen(node.ValueDefinition);
		break;
	
	case NodeKind::BINARY_EXPRESSION:
        result = codegen(node.BinaryExpression);
        break;

    case NodeKind::IDENTIFIER:
        result = codegen(node.Identifier);
        break;

	case NodeKind::LITERAL:
		result = codegen(node.Literal);
		break;

	default:
		assert(false);
		break;
	}
	return result;
}

Value* LLVMCodeGenerator::codegen(NodeID nodeID) {
	Value* result = nullptr;

	// TBD: how do we know if nodeID actually exists?
	// if (ERROR_NODE_ID != NodeBuffers.GetNode(nodeID))
	{
		result = codegen(NodeBuffers.GetNode(nodeID));
	}

	return result;
}

Value* LLVMCodeGenerator::codegen(const VALUE_DEFINITION& node) {
    //
    // Expression of type: [const] identifier = value
    // The identifier can be either global or local to the current function
    // 
    // get the name of the identifier
    const VALUE_DECLARATION& declaration = NodeBuffers.GetNode(node.ValueDefinitionExpressionID).ValueDeclaration;
    const IDENTIFIER& identifier = NodeBuffers.GetNode(declaration.IdentifierID).Identifier;
    std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

    // now get the value by recursively calling codegen
    Value* value = codegen(node.ValueDefinitionExpressionID);

    if (Builder->GetInsertBlock() == nullptr) {
        // If no insertion block, we are creating global variables
        GlobalVariable* gv = new GlobalVariable(*TheModule,
            Type::getDoubleTy(*TheContext),
            (declaration.Kind == VALUE_DECLARATION::Type::Constant),   // isConstant
            GlobalValue::CommonLinkage,
            nullptr,                        // initializer specified below
            Name
        );
        gv->setAlignment(Align(sizeof(double)));

        // currently assuming the initializer is constant
        ConstantFP* ptr_2 = (ConstantFP *)value;
        gv->setInitializer(ptr_2);
    }
    else {
        // add the variable to the end of the insertion block (e.g. function local variables)
        IRBuilder<> TmpB(Builder->GetInsertBlock(), Builder->GetInsertBlock()->end());
        // create a mutable variable
        AllocaInst* A = TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, Name);
        // set the value
        Builder->CreateStore(value, A);
        // store in the local variables map
        NamedValues[Name] = A;
    }

	return value;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::IDENTIFIER& node) {
    //
    // return the value of a local or global variable
    // 
    // Lookup this variable in the current function
    const std::string Name(TokenBuffers.GetValue(node.IdentifierTokenID).ToStringView());
    Value* value = nullptr;
    AllocaInst* A = NamedValues[Name];
    if (A) {
        // Found in the local variables, load the value
        value = Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
    }
    else {
        // not a local variable, check the globals
        GlobalVariable* gv = TheModule->getGlobalVariable(Name);
        if (gv) {
            value = gv->getInitializer();
        }
        else {
            // TBD: LogErrorV("Unknown variable name");
            assert(false);
        }
    }

    return value;
}

Value* LLVMCodeGenerator::codegen(const BINARY_EXPRESSION& node) {
	Value* result = nullptr;
	Value* L = codegen(node.LeftID);
	Value* R = codegen(node.RightID);

	const std::string op(TokenBuffers.GetValue(node.OperatorTokenID).ToStringView());


    if (L && R) {
        if (op == "+")
            result = Builder->CreateFAdd(L, R, "addtmp");
        else if (op == "-")
            result = Builder->CreateFSub(L, R, "subtmp");
        else if (op == "*")
            result = Builder->CreateFMul(L, R, "multmp");
        else if (op == "<") {
            L = Builder->CreateFCmpULT(L, R, "cmptmp");
            // Convert bool 0/1 to double 0.0 or 1.0
            result = Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
        }
        else {
            // TBD
            printf("invalid binary operator\n");
            assert(false);
        }
    }
    return result;
}

Value* LLVMCodeGenerator::codegen(const LITERAL& node) {
	// TBD: currently converting all numbers to float while we improve codegen(const Binary& node) 
	//    return ConstantInt::get(*TheContext, APInt(64, node.Value));
	const std::string val(TokenBuffers.GetValue(node.InfoTokenID).ToStringView());
	return ConstantFP::get(*TheContext, APFloat(atof(val.c_str())));
}
