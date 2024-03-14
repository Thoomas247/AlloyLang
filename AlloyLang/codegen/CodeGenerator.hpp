#pragma once
#include "../parser/Node.hpp"

#include "llvm/llvm.hpp"
#include "NamedValues.hpp"

namespace AlloyCompiler
{
	struct LLVMState
	{
		std::unique_ptr<llvm::LLVMContext> Context;
		std::unique_ptr<llvm::IRBuilder<>> Builder;
		std::unique_ptr<llvm::Module> Module;
		NamedValues NamedValues;

		llvm::AllocaInst* CurrentReturnValue = nullptr;		// keep track of the return value of the current function, the value also includes the type
		llvm::Type* CurrentIdentifierType = nullptr;	// keep track of the type of the last identifier being created or assigned
		llvm::BasicBlock* FuncExitBlock = nullptr;		// to avoid having multiple returns, we simply branch to this BasicBlock

		// handling llvm copde optimizations passes
		std::unique_ptr<llvm::FunctionPassManager> FPM;
		std::unique_ptr<llvm::LoopAnalysisManager> LAM;
		std::unique_ptr<llvm::FunctionAnalysisManager> FAM;
		std::unique_ptr<llvm::CGSCCAnalysisManager> CGAM;
		std::unique_ptr<llvm::ModuleAnalysisManager> MAM;
		std::unique_ptr<llvm::PassInstrumentationCallbacks> PIC;
		std::unique_ptr<llvm::StandardInstrumentations> SI;

		bool Optimizations;

		virtual ~LLVMState()
		{
			assert(true);
		}

		LLVMState(bool optimizations)
			: Optimizations(optimizations)
		{
			Context = std::make_unique<llvm::LLVMContext>();
			Builder = std::make_unique<llvm::IRBuilder<>>(*Context);
			Module = std::make_unique<llvm::Module>("AlloyModule", *Context);

			NamedValues.RegisterDefaultTypes(*Context);

			if (optimizations)
			{
				// check the LLVM tutorial for details about these optimizations
				// create new pass and analysis managers
				FPM = std::make_unique<llvm::FunctionPassManager>();
				LAM = std::make_unique<llvm::LoopAnalysisManager>();
				FAM = std::make_unique<llvm::FunctionAnalysisManager>();
				CGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
				MAM = std::make_unique<llvm::ModuleAnalysisManager>();
				PIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
				SI = std::make_unique<llvm::StandardInstrumentations>(*Context, /*DebugLogging*/ true);

				SI->registerCallbacks(*PIC, MAM.get());

				// add transform passes

				// do simple "peephole" optimizations and bit-twiddling optzns
				FPM->addPass(llvm::InstCombinePass());

				// reassociate expressions
				FPM->addPass(llvm::ReassociatePass());

				// eliminate Common SubExpressions
				FPM->addPass(llvm::GVNPass());

				// simplify the control flow graph (deleting unreachable blocks, etc)
				FPM->addPass(llvm::SimplifyCFGPass());

				// register analysis passes used in these transform passes
				llvm::PassBuilder PB;
				PB.registerModuleAnalyses(*MAM);
				PB.registerFunctionAnalyses(*FAM);
				PB.crossRegisterProxies(*LAM, *FAM, *CGAM, *MAM);
			}
		}
	};

	bool Generate(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state);
	int Execute(LLVMState& state);
}