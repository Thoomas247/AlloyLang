#include "llvm.hpp"
#include "LLVMCodeGenerator.hpp"
#include "NamedValues.hpp"
#include "NamedStructs.hpp"

using namespace llvm;
#ifndef NO_CODE_EXECUTION
using namespace llvm::orc;
#endif

namespace AlloyCompiler
{
LLVMCodeGenerator::LLVMCodeGenerator(const AlloyCompiler::TokenBuffers& tokenBuffers,
	const AlloyCompiler::NodeBuffers& nodeBuffers)
	: NodeBuffers(nodeBuffers), TokenBuffers(tokenBuffers) {

	// Open a new context and module.
	TheContext = std::make_unique<LLVMContext>();
	TheModule = std::make_unique<llvm::Module>("AlloyLang", *TheContext);

	// Create a new builder for the module.
	Builder = std::make_unique<IRBuilder<>>(*TheContext);

	// tree of named values that is passed around the code generator
	RootNamedValues = std::make_unique<CGNamedValues>(*TheContext);
	NamedValues = std::move(RootNamedValues);

#ifndef NO_CODE_OPTIMIZATION
	// Check the LLVM tutorial for details about these optimizations
	// Create new pass and analysis managers.
	TheFPM = std::make_unique<FunctionPassManager>();
	TheLAM = std::make_unique<LoopAnalysisManager>();
	TheFAM = std::make_unique<FunctionAnalysisManager>();
	TheCGAM = std::make_unique<CGSCCAnalysisManager>();
	TheMAM = std::make_unique<ModuleAnalysisManager>();
	ThePIC = std::make_unique<PassInstrumentationCallbacks>();
	TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
		/*DebugLogging*/ true);
	TheSI->registerCallbacks(*ThePIC, TheMAM.get());

	// Add transform passes
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	TheFPM->addPass(InstCombinePass());
	// Reassociate expressions.
	TheFPM->addPass(ReassociatePass());
	// Eliminate Common SubExpressions.
	TheFPM->addPass(GVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	TheFPM->addPass(SimplifyCFGPass());

	// Register analysis passes used in these transform passes.
	PassBuilder PB;
	PB.registerModuleAnalyses(*TheMAM);
	PB.registerFunctionAnalyses(*TheFAM);
	PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
#endif	// NO_CODE_OPTIMIZATION
}

LLVMCodeGenerator::~LLVMCodeGenerator() {
	// values in the satic map are linked to the LLVMContext
	// we need to clear them when the context is destroyed 
	// otherwise we cannot run multiple code generations in the same executable
	CGNamedValues::clearAlloyToLlvmMap();
}

int LLVMCodeGenerator::Process(llvm::raw_ostream* llvmOutput /*= nullptr*/) {
	int result = 0;
	NodeID root = NodeBuffers.GetRootNodeID();
	if (ERROR_NODE_ID == root) {
		// TBD: implement error handling
		assert(false);
		return -1;
	}

	const Node& rootNode = NodeBuffers.GetNode(root);

	result = (codegen(rootNode) ? 0 : -1);
	if (llvmOutput) {
		TheModule->print(*llvmOutput, nullptr);
	}
	else {
		std::error_code EC;
		raw_fd_ostream out("c:\\temp\\out.ll", EC);
		TheModule->print(out, nullptr);
	}
	return result;
}

int LLVMCodeGenerator::Execute(const std::string& MethodName, bool useInterpreter /*= false*/) {
	//
	// execute generated code
	//
	int result = 0;

#ifndef NO_CODE_EXECUTION
	InitializeNativeTarget();
	LLVMInitializeNativeAsmPrinter();

	if (!useInterpreter) {

		// using the recommended LLJIT engine to execute the code
		ExitOnError ExitOnErr;

		// Create an LLJIT instance
		auto J = ExitOnErr(LLJITBuilder().create());
		ExitOnErr(J->addIRModule(ThreadSafeModule(std::move(TheModule), std::move(TheContext))));

		// Look up the JIT'd function, cast it to a function pointer, then call it.
		auto MainAddr = ExitOnErr(J->lookup(MethodName));
		int (*main)() = MainAddr.toPtr<int()>();
		result = main();

	}
	else {
		EngineBuilder	engineBuilder(std::move(TheModule));
		std::string		err;
		engineBuilder.setEngineKind(llvm::EngineKind::Interpreter).setErrorStr(&err);
		ExecutionEngine* executionEngine = engineBuilder.create();
		if (executionEngine) {
			executionEngine->finalizeObject();
			Function* main = executionEngine->FindFunctionNamed(MethodName);
			auto ret = executionEngine->runFunction(main, {});
			delete executionEngine;
			result = (int)ret.IntVal.getSExtValue();
		}
		else {
			assert(false);	// could not instantiate execution engine
		}
	}
#endif // NO_CODE_EXECUTION
	return result;
}

Value* LLVMCodeGenerator::codegen(const Node& node) {

	Value* result = nullptr;

	switch (node.Kind) {

	case NodeKind::LITERAL:							// integer_literal | float_literal | boolean_literal | string_literal | character_literal;
		result = codegen(node.Literal);
		break;

	case NodeKind::IDENTIFIER:						// identifier;
		result = codegen(node.Identifier);
		break;

	case NodeKind::TYPE_IDENTIFIER:					// [reference | pointer] IDENTIFIER;
	case NodeKind::TYPE_DECLARATION:				// [variable | constant] TYPE_IDENTIFIER;
		assert(false);	// To be implemented
		break;

	case NodeKind::VALUE_DECLARATION:				// (variable | constant) IDENTIFIER colon TYPE_IDENTIFIER;
		// nothing to do, handled by VALUE_DEFINITION_EXPRESSION, STRUCT_DEFINITION or FUNCTION_DEFINITION
		result = nullptr;
		break;

	case NodeKind::FUNCTION_DECLARATION:			// function IDENTIFIER open_paren [ VALUE_DECLARATION { comma VALUE_DECLARATION } ] close_paren [ arrow TYPE_DECLARATION ] ;
		result = codegen(node.FunctionDeclaration);
		break;

		/// NodeKind::EXPRESSION group

	case NodeKind::CONSTRUCTOR_EXPRESSION:			// IDENTIFIER open_brace ( IDENTIFIER assignment_operator EXPRESSION ) { comma IDENTIFIER assignment_operator EXPRESSION } close_brace ;
		return codegen(node.ConstructorExpression);
		break;

	case NodeKind::POINTER_INITIALIZER_EXPRESSION:	// new_keyword ( EXPRESSION | ( open_bracket EXPRESSION semicolon EXPRESSION close_bracket ) ) ;
		assert(false);
		break;

	case NodeKind::INITIALIZER_LIST_EXPRESSION:		// open_brace [ EXPRESSION { comma EXPRESSION } ] close_brace ;
		assert(false);
		break;

	case NodeKind::VALUE_DEFINITION_EXPRESSION:		// VALUE_DECLARATION assignment_operator EXPRESSION;
		result = codegen(node.ValueDefinitionExpression);
		break;

	case NodeKind::FUNCTION_CALL_EXPRESSION:		// IDENTIFIER open_paren[EXPRESSION{ comma EXPRESSION }] close_paren;
		result = codegen(node.FunctionCallExpression);
		break;

	case NodeKind::ENCLOSED_EXPRESSION:				// open_paren EXPRESSION close_paren;
		result = codegen(node.EnclosedExpression);
		break;

	case NodeKind::BINARY_EXPRESSION:				// EXPRESSION binary_operator EXPRESSION;
		result = codegen(node.BinaryExpression);
		break;

	case NodeKind::UNARY_EXPRESSION:				// unary_operator PRIMARY_EXPRESSION;
		assert(false);	// To be implemented
		break;

	case NodeKind::ASSIGNMENT_EXPRESSION:			// ( MEMBER_ACCESS_EXPRESSION | IDENTIFIER ) assignment_operator EXPRESSION;
		result = codegen(node.AssignmentExpression);
		break;

	case NodeKind::MEMBER_ACCESS_EXPRESSION:		// IDENTIFIER.IDENTIFIER
		result = codegen(node.MemberAccessExpression);
		break;

		/// End of NodeKind::EXPRESSION group

	/// NodeKind::STATEMENT group

	case NodeKind::ASSIGNMENT_STATEMENT:			// ASSIGNMENT_EXPRESSION semicolon;
		result = codegen(node.AssignmentStatement);
		break;

	case NodeKind::WHILE_LOOP_STATEMENT:			// while ENCLOSED_EXPRESSION STATEMENT;
		result = codegen(node.WhileLoopStatement);
		break;

	case NodeKind::VALUE_DEFINITION_STATEMENT:		// VALUE_DEFINITION_EXPRESSION semicolon;
		result = codegen(node.ValueDefinitionStatement);
		break;

	case NodeKind::FUNCTION_CALL_STATEMENT:			// FUNCTION_CALL_EXPRESSION semicolon
		result = codegen(node.FunctionCallStatement);
		break;

	case NodeKind::FOR_LOOP_STATEMENT:				// for open_paren EXPRESSION semicolon EXPRESSION semicolon EXPRESSION close_paren STATEMENT;
		result = codegen(node.ForLoopStatement);
		break;

	case NodeKind::IF_STATEMENT:					// if ENCLOSED_EXPRESSION STATEMENT[else STATEMENT];
		result = codegen(node.IfStatement);
		break;

	case NodeKind::BLOCK_STATEMENT:					// open_brace{ STATEMENT } close_brace;
		result = codegen(node.BlockStatement);
		break;

	case NodeKind::RETURN_STATEMENT:				// return[EXPRESSION] semicolon;
		result = codegen(node.ReturnStatement);
		break;

		/// End of NodeKind::STATEMENT group

		/// NodeKind::DEFINITION group

	case NodeKind::EXTERN_DEFINITION:				// extern_keyword FUNCTION_DECLARATION semicolon ;
		result = codegen(node.ExternDefinition);
		break;

	case NodeKind::VALUE_DEFINITION:				// VALUE_DEFINITION_EXPRESSION semicolon ;
		result = codegen(node.ValueDefinition);
		break;

	case NodeKind::STRUCT_DEFINITION:				// struct IDENTIFIER open_brace { VALUE_DECLARATION semicolon } close_brace ; (* TODO: force default values? *)
		result = codegen(node.StructDefinition);
		break;

	case NodeKind::ENUM_DEFINITION:					// enum IDENTIFIER open_brace IDENTIFIER { comma IDENTIFIER } close_brace ;
		assert(false);	// To be implemented
		break;

	case NodeKind::FUNCTION_DEFINITION:				// function IDENTIFIER open_paren[VALUE_DECLARATION{ comma VALUE_DECLARATION }] close_paren[arrow TYPE_DECLARATION] BLOCK_STATEMENT;
		result = codegen(node.FunctionDefinition);
		break;

	case NodeKind::QUALIFIED_DEFINITION:
		result = codegen(node.QualifiedDefinition.DefinitionID);
		break;

		/// End of NodeKind::DEFINITION group

	case NodeKind::MODULE:
	{
		std::vector<NodeID>::const_iterator iter = node.Module.QualifiedDefinitionIDs.begin();
		for (iter; iter < node.Module.QualifiedDefinitionIDs.end(); iter++) {
			codegen(*iter);
		}
		break;
	}

	case NodeKind::PROGRAM:
	{
		std::vector<NodeID>::const_iterator iter = node.Program.ModuleIDs.begin();
		for (iter; iter < node.Program.ModuleIDs.end(); iter++) {
			codegen(*iter);
		}
		break;
	}

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
	// Expression of type: [const] identifier = value;	(with the semi-colon at the end)
	//
	assert(false);	// TBD: this should return a value of type True or False
	return codegen(node.ValueDefinitionExpressionID);
}

Value* LLVMCodeGenerator::codegen(const STRUCT_DEFINITION& node) {
	//
	// Expression of type: struct IDENTIFIER { VALUE_DECLARATION semicolon }
	// Returns only True (1) or False (0)
	//

	Value* result = nullptr;

	// get the name of the structure
	const IDENTIFIER& identifier = NodeBuffers.GetNode(node.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	// get a vector of member types
	std::vector<Type*> MemberTypes;
	std::map<std::string, int> memberNames;
	int memberIndex = 0;
	for (auto id : node.MemberIDs) {
		assert(NodeBuffers.GetNode(id).Kind == NodeKind::VALUE_DECLARATION);
		const VALUE_DECLARATION& vd = NodeBuffers.GetNode(id).ValueDeclaration;
		llvm::Type* type = CGNamedValues::AlloyToLLVMType(*TheContext, NodeBuffers, TokenBuffers, vd.IdentifierOrTypeIdentifierID);
		MemberTypes.push_back(type);

		// retrieve member name and add it to memberNames map
		const IDENTIFIER& memberID = NodeBuffers.GetNode(vd.IdentifierID).Identifier;
		std::string memberName(TokenBuffers.GetValue(memberID.IdentifierTokenID).ToStringView());
		memberNames[memberName] = memberIndex++;
	}

	// store a map to member names
	NamedStructs::setMemberNames(Name, memberNames);

	// now create the structure type in llvm
	llvm::StructType* structType = StructType::create(*TheContext, MemberTypes, Name);
	if (nullptr != structType) {

		// add the newly created type tot he AlloyToLLVM map
		CGNamedValues::addType(Name, StructType::getTypeByName(*TheContext, Name));
		result = ConstantInt::getTrue(*TheContext);

	}
	else {
		result = ConstantInt::getFalse(*TheContext);
	}

	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::CONSTRUCTOR_EXPRESSION& node) {
	//
	// IDENTIFIER { ( IDENTIFIER = EXPRESSION ) { comma IDENTIFIER = EXPRESSION } } ;
	//
	//
	Value* result = NamedStructs::getMutableStruct(*this,
		node.StructIdentifierID,
		node.MemberIdentifierIDs,
		node.MemberValueIDs);

	return result;
}

Value* LLVMCodeGenerator::codegen(const VALUE_DEFINITION_EXPRESSION& node) {
	//
	// Expression of type: [const] identifier = value
	// The identifier can be either global or local to the current function
	// 
	// get the name of the identifier
	assert(NodeBuffers.GetNode(node.ValueDeclarationID).Kind == NodeKind::VALUE_DECLARATION);
	const VALUE_DECLARATION& declaration = NodeBuffers.GetNode(node.ValueDeclarationID).ValueDeclaration;
	const IDENTIFIER& identifier = NodeBuffers.GetNode(declaration.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	// now get the value by recursively calling codegen
	Value* value = codegen(node.ValueID);

	if (Builder->GetInsertBlock() == nullptr) {
		// If no insertion block, we are creating global variables
		GlobalVariable* gv = new GlobalVariable(*TheModule,
			value->getType(),
			(declaration.Kind == VALUE_DECLARATION::Type::Constant),   // isConstant
			GlobalValue::InternalLinkage,	// TBD: check CLang source-code the proper options for creating globals
			nullptr,                        // initializer specified below
			Name
		);
		gv->setAlignment(Align(sizeof(double)));
		gv->setDSOLocal(true);

		// currently assuming the initializer is constant
		if (isa<Constant>(value)) {
			Constant* ptr_2 = static_cast<Constant*>(value);
			gv->setInitializer(ptr_2);
		}
		else {
			// TBD: should convert value to constant
			assert(false);
		}
	}
	else {
		// add the variable to the end of the insertion block (e.g. function local variables)
		IRBuilder<> TmpB(Builder->GetInsertBlock(), Builder->GetInsertBlock()->end());
		// create a mutable variable
		AllocaInst* A = TmpB.CreateAlloca(value->getType(), nullptr, Name);
		// set the value
		Builder->CreateStore(value, A);
		// store in the local variables map
		NamedValues->insert(Name, A);
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
	if (!loadValueOfLocalOrGlobalVariable(Name, value)) {
		// TBD: LogErrorV("Unknown variable name");
		assert(false);
	}

	return value;
}

bool LLVMCodeGenerator::updateValueOfLocalOrGlobalVariable(const std::string& Name, Value* Value) {
	//
	// update the value of a local or global variable
	//
	bool result = false;

	// Lookup this variable in the current function
	AllocaInst* A = NamedValues->contains(Name, true);
	if (A) {
		// Found in the local variables, save the value
		result = (Builder->CreateStore(Value, A) != nullptr);
	}
	else {
		// not a local variable, check the globals
		GlobalVariable* gv = TheModule->getGlobalVariable(Name);
		if (gv) {
			if (isa<Constant>(Value)) {
				gv->setInitializer(static_cast<Constant*>(Value));
			}
			else {
				// need to convert initializer to constant
				assert(false);
			}
			result = true;
		}
		else {
			// TBD: Unknown variable name
			assert(false);
		}
	}

	return result;
}

Value* LLVMCodeGenerator::loadValueOfLocalOrGlobalVariable(const std::string& Name, Value*& Value) {
	//
	// read the value of a local or global variable
	// returns the pointer to the local or global variable or nullptr if not found
	// 
	llvm::Value* ptr = nullptr;

	AllocaInst* A = NamedValues->contains(Name, true);
	GlobalVariable* gv = nullptr;
	if (A) {
		// Found in the local variables, load the value
		Value = Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
		ptr = A;
	}
	else {
		// not a local variable, check the globals
		gv = TheModule->getGlobalVariable(Name);
		if (gv) {
			Value = gv->getInitializer();
			ptr = gv;
		}
		else {
			// TBD: Unknown variable name
			assert(false);
		}
	}

	return ptr;
}

Value* LLVMCodeGenerator::codegen(const BINARY_EXPRESSION& node) {
	//
	// TBD: This needs to be written properly for various operation and value types
	// TBD: Use a map to map Alloy operator tokens to llvm Instruction::BinaryOps enum then call ConstantFoldBinaryInstruction or similar
	//

	Value* result = nullptr;
	const std::string op(TokenBuffers.GetValue(node.OperatorTokenID).ToStringView());

	if (op == "=") {
		// special case for the assignment operator
		// we encounter this in cases such as the last expression of: for (var i : i64 = 0; i < c; i = i + 1)
		ASSIGNMENT_EXPRESSION expr = { node.LeftID, node.OperatorTokenID, node.RightID };
		result = codegen(expr);
	}
	else {
		Value* L = codegen(node.LeftID);
		Value* R = codegen(node.RightID);

		// we need the 2 operands to be of the same type, if not convert to same
		Type* operandType = CGNamedValues::MakeCompatible(L, R);

		if (L && R) {
			if (op == "+")
				// example for handling both integer and floating additions
				result = (operandType->getTypeID() == Type::TypeID::IntegerTyID ? Builder->CreateAdd(L, R, "addtmp") : Builder->CreateFAdd(L, R, "addtmp"));
			else if (op == "-")
				result = (operandType->getTypeID() == Type::TypeID::IntegerTyID ? Builder->CreateSub(L, R, "subtmp") : Builder->CreateFSub(L, R, "subtmp"));
			else if (op == "*")
				// example for handling both integer and floating multiplications
				result = (operandType->getTypeID() == Type::TypeID::IntegerTyID ? Builder->CreateMul(L, R, "multmp") : Builder->CreateFMul(L, R, "multmp"));
			else if (op == "==") {
				// example for handling both integer and floating comparisons
				L = (operandType->getTypeID() == Type::TypeID::IntegerTyID ? Builder->CreateICmpEQ(L, R, "equtmp") : Builder->CreateFCmpOEQ(L, R, "equtmp"));
				// Convert bool 0/1 to double 0.0 or 1.0
				result = Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
			}
			else if (op == "<") {
				// example for handling both integer and floating comparisons
				L = (operandType->getTypeID() == Type::TypeID::IntegerTyID ? Builder->CreateICmpSLT(L, R, "cmptmp") : Builder->CreateFCmpULT(L, R, "cmptmp"));
				// Convert bool 0/1 to double 0.0 or 1.0
				result = Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
			}
			else {
				// TBD
				printf("invalid binary operator\n");
				assert(false);
			}
		}
	}
	return result;
}

Value* LLVMCodeGenerator::codegen(const LITERAL& node) {
	//
	// Convert a literal to an LLVM Value
	//
	Value* result = nullptr;

	const std::string val(TokenBuffers.GetValue(node.InfoTokenID).ToStringView());
	switch (node.Kind) {
	case LITERAL::Type::Integer:
		// TBD: assuming all integers are 64 bit and base 10 encoded
		result = ConstantInt::get(*TheContext, APInt(64, val, 10));
		break;

	case LITERAL::Type::Float:
		result = ConstantFP::get(*TheContext, APFloat(atof(val.c_str())));
		break;

	case LITERAL::Type::String:
		result = Builder->CreateGlobalStringPtr(val, "stringval");
		break;

	case LITERAL::Type::Boolean:
	case LITERAL::Type::Character:
	default:
		// TBD: implement other case
		break;
	}

	return result;
}

std::string LLVMCodeGenerator::UnescapeString(const AlloyCompiler::SmallStringView& str)
{
	static const std::unordered_map<char, char> escapeCharacterMap =
	{
		{ 'n',	'\n' },
		{ 'r',	'\r' },
		{ 't',	'\t' },
		{ 'v',	'\v' },
		{ '\\', '\\' },
		{ '\'', '\'' },
		{ '\"', '\"' },
		{ '0',	'\0' },
		{ 's',	' ' }
	};

	std::string result;
	result.reserve(str.Size());

	for (size_t i = 0; i < str.Size(); i++)
	{
		char c = str[i];

		if (c == '\\' && i + 1 < str.Size())
		{
			i++;

			auto it = escapeCharacterMap.find(str[i]);

			if (it != escapeCharacterMap.end())
			{
				c = it->second;
			}

			else
			{
				continue;	// TODO: log error?
			}
		}

		result.push_back(c);
	}

	return result;
}

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
AllocaInst* LLVMCodeGenerator::CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName, llvm::Type* type) {
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(type, nullptr, VarName);
}


Function* LLVMCodeGenerator::createFunctionPrototype(const std::string& Name, const AlloyCompiler::FUNCTION_DECLARATION& node) {
	//
	// Helper method to create the protoype of a function
	//

	// convert each of the parameter types into llvm types
	std::vector<Type*> ParamTypes;
	for (auto id : node.ParameterIDs) {
		assert(NodeBuffers.GetNode(id).Kind == NodeKind::VALUE_DECLARATION);
		const VALUE_DECLARATION& vd = NodeBuffers.GetNode(id).ValueDeclaration;
		llvm::Type* type = CGNamedValues::AlloyToLLVMType(*TheContext, NodeBuffers, TokenBuffers, vd.IdentifierOrTypeIdentifierID);
		ParamTypes.push_back(type);
	}

	// now convert the return type (if any) to llvm
	Type* returnType = nullptr;
	if (node.ReturnTypeID != ERROR_NODE_ID) {
		const TYPE_DECLARATION& td = NodeBuffers.GetNode(node.ReturnTypeID).TypeDeclaration;
		returnType = CGNamedValues::AlloyToLLVMType(*TheContext, NodeBuffers, TokenBuffers, td.IdentifierOrTypeIdentifierID);
	}
	else {
		returnType = Type::getVoidTy(*TheContext);
	}

	// Make the function type: return type(param type, param type, ...)
	// TBD: the last parameter should be true only for variable number of parmeters (e.g. printf), currently assuming all functions are variable parameters
	FunctionType* FT = FunctionType::get(returnType, ParamTypes, true);

	Function* F = Function::Create(FT, Function::ExternalLinkage, Name, *TheModule);

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto& Arg : F->args()) {
		assert(NodeBuffers.GetNode(node.ParameterIDs[Idx]).Kind == NodeKind::VALUE_DECLARATION);		// make sure we are getting back the right node type
		const VALUE_DECLARATION& temp = NodeBuffers.GetNode(node.ParameterIDs[Idx]).ValueDeclaration;
		const IDENTIFIER& identifier = NodeBuffers.GetNode(temp.IdentifierID).Identifier;
		std::string argName(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());
		Arg.setName(argName);
		Idx++;
	}

	return F;
}

Function* LLVMCodeGenerator::codegen(const AlloyCompiler::FUNCTION_DECLARATION& node) {
	//
	// function declaration in the form of fn IDENTIFIER ( parameter, parameter, ... ) -> return type;
	//
	// retrieve the function name
	assert(NodeBuffers.GetNode(node.IdentifierID).Kind == NodeKind::IDENTIFIER);		// make sure we are getting back the right node type
	const IDENTIFIER& identifier = NodeBuffers.GetNode(node.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	// First, check for an existing function from a previous declaration
	Function* F = TheModule->getFunction(Name);

	if (F) {
		// TBD check for function signature
		// devise a way to have multiple function with same name but different parameters
		assert(false);
		return nullptr;
	}

	// create the llvm function prototype
	F = createFunctionPrototype(Name, node);
	if (!F) {
		// Error creating the prototype
		assert(false);
		return nullptr;
	}

	return F;
}

Function* LLVMCodeGenerator::codegen(const AlloyCompiler::FUNCTION_DEFINITION& node) {
	//
	// Function definition in the form of fn function( prarameter, parameter, ... ) ->  type { statements }
	//

	Function* F = static_cast<llvm::Function*>(codegen(node.FunctionDeclarationID));

	// Create a new basic block to start insertion into.
	BasicBlock* BB = BasicBlock::Create(*TheContext, "entry", F);
	Builder->SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues->clear();
	for (auto& Arg : F->args()) {
		// Create an alloca for this variable.
		AllocaInst* Alloca = CreateEntryBlockAlloca(F, std::string(Arg.getName()), Arg.getType());

		// Store the initial value into the alloca.
		Builder->CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues->insert(std::string(Arg.getName()), Alloca);
	}

	// create a new local names map for the function body
	NamedValues = std::make_unique<CGNamedValues>(*TheContext, std::move(NamedValues));

	ConstantInt* retval = static_cast<ConstantInt*>(codegen(node.BodyID));

	// if there is no return instruction before the end of a function, llvm generates an error
	// e.g.: fn main() -> i64 { if (1.0 == 1.0) return 1; else return 0; } generates at the end of main: ifcont: } which is an error 
	// even if we are actually returning a value
	if (BB->getTerminator() == nullptr
		|| !isa<ReturnInst>(BB->getTerminator())) {
		// TBI: tricky situation, we cannot force a return instruction because we might be returning an unintended value
	}

	if (retval && retval->isOne()) {
		// Validate the generated code, checking for consistency.
		verifyFunction(*F);

#ifndef NO_CODE_OPTIMIZATION
		// Run the optimizer on the function.
		TheFPM->run(*F, *TheFAM);
#endif  // NO_CODE_OPTIMIZATION
	}
	else {
		// Error reading body, remove function
		assert(false);
		F->eraseFromParent();
		F = nullptr;
	}

	// restore the named values of the higher level
	NamedValues = std::move(NamedValues->getParent());

	return F;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::BLOCK_STATEMENT& node) {
	//
	// Block of code statements { statement; statement; ... }
	// returns ConstantInt True or False depending on success or failure
	//
	Value* result = ConstantInt::getTrue(*TheContext);

	// create a new local names map for each block statement, any variables declared inside the block are limited to this scope
	NamedValues = std::make_unique<CGNamedValues>(*TheContext, std::move(NamedValues));

	for (const NodeID& S : node.StatementIDs) {

		Value* stmtResult = codegen(S);
		if (stmtResult == nullptr 
			|| !isa<ConstantInt>(stmtResult) 
			|| static_cast<ConstantInt *>(stmtResult)->isZero()
			) {
			assert(false);
			result = ConstantInt::getFalse(*TheContext);
			break;
		}

		if (NodeKind::RETURN_STATEMENT == NodeBuffers.GetNode(S).Kind) {
			// TBD: generate a warning that we encountered unreachable statements
			break;			// ignore any statements after return

		}
	}

	// restore the named values of the higher level
	NamedValues = std::move(NamedValues->getParent());

	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::MEMBER_ACCESS_EXPRESSION& node, Value** ObjectPtr /*= nullptr*/) {
	//
	// return the value of a single member of a local or global struct variable
	// 

	Value* value = nullptr;
	Value* ptr = nullptr;

	assert(NodeBuffers.GetNode(node.RightID).Kind == NodeKind::IDENTIFIER);
	const IDENTIFIER& right = NodeBuffers.GetNode(node.RightID).Identifier;
	std::string MemberName(TokenBuffers.GetValue(right.IdentifierTokenID).ToStringView());

	switch (NodeBuffers.GetNode(node.LeftID).Kind) {
	case NodeKind::IDENTIFIER:
	{
		// Lookup this variable in the current function
		const IDENTIFIER& left = NodeBuffers.GetNode(node.LeftID).Identifier;
		std::string ObjectName(TokenBuffers.GetValue(left.IdentifierTokenID).ToStringView());

		ptr = NamedStructs::loadValueOfLocalOrGlobalStructMember(*this, ObjectName, MemberName, value);
		break;
	}

	case NodeKind::MEMBER_ACCESS_EXPRESSION:
	{
		Value* ObjectPtr = nullptr;
		Value* MemberValue = codegen(NodeBuffers.GetNode(node.LeftID).MemberAccessExpression, &ObjectPtr);
		if (MemberValue != nullptr && ObjectPtr != nullptr) {
			// then get the value of the object's member
			value = MemberValue;
			ptr = NamedStructs::loadValueOfLocalOrGlobalStructMember(*this, ObjectPtr, MemberName, value);
		}
		else {
			assert(false);	// TBD: could not evaluate structure member
		}
		break;
	}

	default:
		// TBD: unexpected left ID
		assert(false);
		break;
	}

	if (nullptr == ptr) {
		// TBD: Unknown variable or member name
		assert(false);
	}
	// in addition to the value, return a pointer to the object or the member
	if (ObjectPtr != nullptr) {
		*ObjectPtr = ptr;
	}
	return value;
}


Value* LLVMCodeGenerator::codegen(const AlloyCompiler::ASSIGNMENT_EXPRESSION& node) {
	//
	// identifier = expression
	//
	// TBD: what is ASSIGNMENT_EXPRESSION.OperatorTokenID for?
	Value* result = nullptr;

	switch (NodeBuffers.GetNode(node.IdentifierOrMemberAccessID).Kind) {
	case NodeKind::IDENTIFIER:
	{
		const IDENTIFIER& identifier = NodeBuffers.GetNode(node.IdentifierOrMemberAccessID).Identifier;
		std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

		// now evaluate the expression by recursively calling codegen
		result = codegen(node.ValueID);

		if (!updateValueOfLocalOrGlobalVariable(Name, result)) {
			// error updating the value of local or global variable
			assert(false);
		}

		break;
	}

	case NodeKind::MEMBER_ACCESS_EXPRESSION:
	{
		const MEMBER_ACCESS_EXPRESSION& memberAccess = NodeBuffers.GetNode(node.IdentifierOrMemberAccessID).MemberAccessExpression;

		Value* structVariable = nullptr;
		Value* structValue = nullptr;

		const IDENTIFIER& right = NodeBuffers.GetNode(memberAccess.RightID).Identifier;
		std::string MemberName(TokenBuffers.GetValue(right.IdentifierTokenID).ToStringView());

		const Node& leftNode = NodeBuffers.GetNode(memberAccess.LeftID);
		if (leftNode.Kind == NodeKind::IDENTIFIER) {
			const IDENTIFIER& left = leftNode.Identifier;
			std::string StructName(TokenBuffers.GetValue(left.IdentifierTokenID).ToStringView());

			// Lookup this variable in the current function and load the previous value
			// loading the previous value (which is a structure) gives us the details about the structure
			structVariable = loadValueOfLocalOrGlobalVariable(StructName, structValue);
			if (!structVariable) {
				// TBD: Unknown variable name
				assert(false);
			}

			// now evaluate the right-hand expression by recursively calling codegen
			result = codegen(node.ValueID);

			if (!NamedStructs::updateValueOfLocalOrGlobalStructMember(*this, structVariable, structValue, MemberName, result)) {
				// error updating the value of local or global variable
				assert(false);
			}
		}
		else {
			assert(leftNode.Kind == NodeKind::MEMBER_ACCESS_EXPRESSION);	// TBD: if it is not an identifier, it must be another member access

			const MEMBER_ACCESS_EXPRESSION& left = leftNode.MemberAccessExpression;

			// recursively call the same method till we get to the initial structure
			Value* ObjectPtr = nullptr;
			Value* MemberValue = codegen(left, &ObjectPtr);
			if (MemberValue != nullptr && ObjectPtr != nullptr) {

				// evaluate the right-hand expression by recursively calling codegen
				result = codegen(node.ValueID);

				// then set the value of the object's member
				if (!NamedStructs::updateValueOfLocalOrGlobalStructMember(*this, ObjectPtr, MemberValue, MemberName, result)) {
					// error updating the value of local or global object member
					assert(false);
				}
			}
		}

		break;
	}

	default:			// TBD: wrong node type
		assert(false);
		break;

	}
	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::FUNCTION_CALL_EXPRESSION& node) {
	//
	// Function call: identifier(expression, expression, ...)
	//
	assert(NodeBuffers.GetNode(node.IdentifierID).Kind == NodeKind::IDENTIFIER);		// make sure we are getting back the right node type
	const IDENTIFIER& identifier = NodeBuffers.GetNode(node.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	// Look up the name in the global module table.
	Function* CalleeF = TheModule->getFunction(Name);
	if (!CalleeF) {
		// Unknown function referenced
		assert(false);
		return nullptr;
	}

	// If argument mismatch error.
	if (CalleeF->arg_size() != node.ArgumentIDs.size()) {
		// Incorrect # arguments passed
		assert(false);
		return nullptr;
	}

	// evaluate all arguments by calling codegen on each expression
	std::vector<Value*> ArgsV;
	for (size_t i = 0, e = node.ArgumentIDs.size(); i != e; ++i) {
		ArgsV.push_back(codegen(node.ArgumentIDs[i]));
		if (!ArgsV.back())
			return nullptr;
	}

	return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::IF_STATEMENT& node) {
	//
	// if (expression) statements [else statements]
	// returns ConstantInt True or False depending on success or failure
	//
	Value* result = ConstantInt::getFalse(*TheContext);

	Value* CondV = codegen(node.ConditionExpressionID);
	if (!CondV) {
		// error evaluating condition
		assert(false);
		return result;
	}

	// Convert condition to a bool by comparing non-equal to 0
	// TBI: extend to all possible data types
	if (CondV->getType()->getTypeID() == Type::TypeID::IntegerTyID) {
		CondV = Builder->CreateICmpEQ(CondV, ConstantInt::get(*TheContext, APInt(64, 0)), "ifcond");
	}
	else {
		CondV = Builder->CreateFCmpONE(CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "ifcond");
	}

	// we are supposed to be inside a function, otherwise this is going to fail
	Function* TheFunction = Builder->GetInsertBlock()->getParent();
	if (!TheFunction) {
		// not inside a function or something missing in the codegen of the function
		assert(false);
		return result;
	}

	// Create blocks for the then and else cases
	// Insert the 'then' block at the end of the function
	BasicBlock* ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
	// the else block is optional but we still need an empty else statement otherwise the optimizer crashes
	BasicBlock* ElseBB = BasicBlock::Create(*TheContext, "else");
	BasicBlock* MergeBB = BasicBlock::Create(*TheContext, "ifcont");
	Builder->CreateCondBr(CondV, ThenBB, ElseBB);

	// Emit then statements
	Builder->SetInsertPoint(ThenBB);
	Value* ThenV = codegen(node.BodyID);
	if (ThenV == nullptr
		|| !isa<ConstantInt>(ThenV)
		|| static_cast<ConstantInt*>(ThenV)->isZero()
		) {
		assert(false);
		return result;
	}

	Builder->CreateBr(MergeBB);

	// Emit else block if any
	Value* ElseV = nullptr;
	if (ElseBB) {
		TheFunction->insert(TheFunction->end(), ElseBB);
		Builder->SetInsertPoint(ElseBB);
		if (ERROR_NODE_ID == node.ElseID) {
			// no else statement, return success
			result = ConstantInt::getTrue(*TheContext);;
		}
		else {
			ElseV = codegen(node.ElseID);
			if (ElseV == nullptr
				|| !isa<ConstantInt>(ElseV)
				|| static_cast<ConstantInt*>(ElseV)->isZero()
				) {
				assert(false);
				return result;
			}
		}
	}

	Builder->CreateBr(MergeBB);

	// Emit merge block.
	TheFunction->insert(TheFunction->end(), MergeBB);
	Builder->SetInsertPoint(MergeBB);

	result = ConstantInt::getTrue(*TheContext);
	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::FOR_LOOP_STATEMENT& node) {
	// 
	// for ( EXPRESSION; EXPRESSION; EXPRESSION ) STATEMENT 
	// returns ConstantInt True or False depending on success or failure
	// 
	// Output for-loop as:
	//   var = alloca double
	//   ...
	//   start = startexpr
	//   store start -> var
	//   goto loop
	// loop:
	//   ...
	//   bodyexpr
	//   ...
	// loopend:
	//   step = stepexpr
	//   endcond = endexpr
	//
	//   curvar = load var
	//   nextvar = curvar + step
	//   store nextvar -> var
	//   br endcond, loop, endloop
	// outloop:

	ConstantInt* result = ConstantInt::getFalse(*TheContext);

	// we are supposed to be inside a function, otherwise this is going to fail
	Function* TheFunction = Builder->GetInsertBlock()->getParent();
	if (!TheFunction) {
		// not inside a function or something missing in the codegen of the function
		assert(false);
		return result;
	}

	// create a new local names map for each block statement, any variables declared inside the block are limited to this scope
	NamedValues = std::make_unique<CGNamedValues>(*TheContext, std::move(NamedValues));

	// Emit the start code first if any.
	if (ERROR_NODE_ID != node.InitExpressionID) {
		if (!codegen(node.InitExpressionID)) {
			// error evaluating start expression
			assert(false);

			// restore the named values of the higher level
			NamedValues = std::move(NamedValues->getParent());

			return result;
		}
	}

	// Make the new basic block for the loop header, inserting after current block.
	BasicBlock* LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);

	// Insert an explicit fall through from the current block to the LoopBB
	// (why is this needed is an LLVM mystery)
	Builder->CreateBr(LoopBB);

	// Start insertion in LoopBB.
	Builder->SetInsertPoint(LoopBB);

	// Emit the body of the loop.  This, like any other expr, can change the
	// current BB.  Note that we ignore the value computed by the body, but don't
	// allow an error.
	Value* stmtResult = codegen(node.BodyID);
	result = static_cast<ConstantInt*>(stmtResult);
	if (nullptr == stmtResult
		|| !isa<ConstantInt>(stmtResult)
		|| result->isZero()
		) {
		assert(false);
		return result;
	}

	// Emit the (optional) step value.
	Value* StepVal = nullptr;
	if (ERROR_NODE_ID != node.IncrementExpressionID) {
		StepVal = codegen(node.IncrementExpressionID);
		if (!StepVal) {
			// error evaluating expression
			assert(false);

			// restore the named values of the higher level
			NamedValues = std::move(NamedValues->getParent());

			return result;
		}
	}

	// Compute the end condition.
	Value* EndCond = codegen(node.ConditionExpressionID);
	if (!EndCond) {
		// error evaluating end expression
		assert(false);

		// restore the named values of the higher level
		NamedValues = std::move(NamedValues->getParent());

		return result;
	}

	// Convert condition to a bool by comparing non-equal to 0.0.
	EndCond = Builder->CreateFCmpONE(EndCond, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

	// Create the "after loop" block and insert it.
	BasicBlock* AfterBB = BasicBlock::Create(*TheContext, "afterloop", TheFunction);

	// Insert the conditional branch into the end of LoopEndBB.
	Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

	// Any new code will be inserted in AfterBB.
	Builder->SetInsertPoint(AfterBB);

	// for expr always returns nullptr
	return result;
}

Value* LLVMCodeGenerator::codegen(const WHILE_LOOP_STATEMENT& node) {
	//
	// while (expression) statement;
	//
	// Output while-loop as:
	// goto loop
	// loop:
	// endcond = condexpr
	// br endcond, bodyloop, outloop
	// bodyloop:
	// ...
	// bodystmts
	// ...
	// goto loop
	// outloop:
	//
	ConstantInt* result = ConstantInt::getFalse(*TheContext);

	// we are supposed to be inside a function, otherwise this is going to fail
	Function* TheFunction = Builder->GetInsertBlock()->getParent();
	if (!TheFunction) {
		// not inside a function or something missing in the codegen of the function
		assert(false);
		return result;
	}

	// create a new local names map for each block statement, any variables declared inside the block are limited to this scope
	NamedValues = std::make_unique<CGNamedValues>(*TheContext, std::move(NamedValues));

	// Make the new basic block for the loop header, inserting after current block.
	BasicBlock* LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);

	// Insert an explicit fall through from the current block to the LoopBB
	// (why is this needed is an LLVM mystery)
	Builder->CreateBr(LoopBB);

	// Start insertion in LoopBB.
	Builder->SetInsertPoint(LoopBB);

	// Compute the end condition
	Value* EndCond = codegen(node.ConditionExpressionID);
	if (!EndCond) {
		// error evaluating end expression
		assert(false);

		// restore the named values of the higher level
		NamedValues = std::move(NamedValues->getParent());

		return result;
	}

	// Convert condition to a bool by comparing non-equal to 0.0.
	EndCond = Builder->CreateFCmpONE(EndCond, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

	// Create the "body loop" block
	BasicBlock* BodyBB = BasicBlock::Create(*TheContext, "bodyloop", TheFunction);

	// Create the "out loop" block
	BasicBlock* OutBB = BasicBlock::Create(*TheContext, "outloop", TheFunction);

	// Insert the conditional branch
	Builder->CreateCondBr(EndCond, BodyBB, OutBB);

	// Any new code will be inserted in BodyBB
	Builder->SetInsertPoint(BodyBB);

	// Emit the body of the loop.  This, like any other expr, can change the
	// current BB.  Note that we ignore the value computed by the body, but don't
	// allow an error.
	Value* stmtResult = codegen(node.BodyID);
	result = static_cast<ConstantInt*>(stmtResult);
	if (nullptr == stmtResult
		|| !isa<ConstantInt>(stmtResult)
		|| result->isZero()
		) {
		assert(false);
		return result;
	}

	// Unconditional branch to start of loop
	Builder->CreateBr(LoopBB);

	// Any new code will be inserted in OutBB.
	Builder->SetInsertPoint(OutBB);

	// while expr always returns nullptr
	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::RETURN_STATEMENT& node) {
	//
	// return expression;
	// returns ConstantInt True or False depending on success or failure
	//
	Value* result = ConstantInt::getFalse(*TheContext);
	if (ERROR_NODE_ID == node.ExpressionID) {
		Builder->CreateRetVoid();
		result = ConstantInt::getTrue(*TheContext);
	}
	else {
		result = codegen(node.ExpressionID);
		if (result != nullptr) {
			Builder->CreateRet(result);
			result = ConstantInt::getTrue(*TheContext);
		}
	}
	return result;
}

}	// end of namespace AlloyCompiler