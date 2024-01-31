#include <vector>
#include "LLVMCodeGenerator.hpp"

using namespace llvm;
using namespace AlloyCompiler;

LLVMCodeGenerator::LLVMCodeGenerator(const Parser::NodeDataBuffers& nodeDataBuffers)
    : NodeBuffers(nodeDataBuffers) {

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
    for (auto& Arg : F->args())
        NamedValues[std::string(Arg.getName())] = &Arg;
    
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
    case NodeKind::Program:
    {
        std::vector<NodeID>::const_iterator iter = node.Program.Modules.List.begin();
        for (iter; iter < node.Program.Modules.List.end(); iter++) {
            codegen(*iter);
        }
        break;
    }

    case NodeKind::Module:
    {
        std::vector<NodeID>::const_iterator iter = node.Module.QualifiedDefinitions.List.begin();
        for (iter; iter < node.Module.QualifiedDefinitions.List.end(); iter++) {
            codegen(*iter);
        }
        break;
    }

    case NodeKind::QualifiedDefinition:
        codegen(node.QualifiedDefinition.Definition);
        break;

    case NodeKind::ConstantDefinition:
    case NodeKind::VariableDefinition:
        codegen(node.VariableDefinition);
        break;

    case NodeKind::IntegerLiteral:
        result = codegen(node.IntegerLiteral);
        break;

    case NodeKind::Binary:
        result = codegen(node.Binary);
        break;

    default:
        assert(false);
        break;
    }
    return result;
}

Value* LLVMCodeGenerator::codegen(uint32_t nodeID) {
    Value* result = nullptr;

    // TBD: how do we know if nodeID actually exists?
    // if (ERROR_NODE_ID != NodeBuffers.GetNode(nodeID))
    {
        result = codegen(NodeBuffers.GetNode(nodeID));
    }

    return result;
}

Value* LLVMCodeGenerator::codegen(const VariableDefinition& node) {
    const Node& expression = NodeBuffers.GetNode(node.Value);
    return HandleTopLevelExpression(expression);
}

Value* LLVMCodeGenerator::codegen(const Binary& node) {
    Value* result = nullptr;
    Value* L = codegen(node.Left);
    Value* R = codegen(node.Right);
    if (L && R) {
        switch (node.Op) {
        case TokenValue::Plus:
            return Builder->CreateFAdd(L, R, "addtmp");
        case TokenValue::Minus:
            return Builder->CreateFSub(L, R, "subtmp");
        case TokenValue::Multiply:
            return Builder->CreateFMul(L, R, "multmp");
        case TokenValue::LessThan:
            L = Builder->CreateFCmpULT(L, R, "cmptmp");
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
        default:
            // TBD
            printf("invalid binary operator\n");
            return 0;
        }
    }
    return result;
}

Value* LLVMCodeGenerator::codegen(const IntegerLiteral& node) {
//    return ConstantInt::get(*TheContext, APInt(64, node.Value));
    return ConstantFP::get(*TheContext, APFloat((double)node.Value));
}
