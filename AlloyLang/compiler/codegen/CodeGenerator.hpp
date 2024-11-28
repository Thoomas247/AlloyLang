#pragma once

#include "../ModuleTable.hpp"

namespace AlloyCompiler
{
	struct LLVMState
	{
		std::unique_ptr<llvm::LLVMContext> Context;
		std::unique_ptr<llvm::IRBuilder<>> Builder;
		std::unique_ptr<llvm::Module> Module;
		NamedValues NamedValues;
		std::string MainFunctionName;

		llvm::AllocaInst* CurrentReturnValue = nullptr;		// keep track of the return value of the current function, the value also includes the type
		llvm::BasicBlock* FuncExitBlock = nullptr;		// to avoid having multiple returns, we simply branch to this BasicBlock

		// this map will contain for each smart pointer type, the underlying element type and whether this is an array or just a pointer
		std::unordered_map<llvm::Type*, std::tuple<llvm::Type*, bool>> smartPointerTypeMap;

		// handling llvm code optimization passes
		std::unique_ptr<llvm::FunctionPassManager> FPM;
		std::unique_ptr<llvm::LoopAnalysisManager> LAM;
		std::unique_ptr<llvm::FunctionAnalysisManager> FAM;
		std::unique_ptr<llvm::CGSCCAnalysisManager> CGAM;
		std::unique_ptr<llvm::ModuleAnalysisManager> MAM;
		std::unique_ptr<llvm::PassInstrumentationCallbacks> PIC;
		std::unique_ptr<llvm::StandardInstrumentations> SI;

		llvm::ModulePassManager MPM;

		typedef enum { OptimizeNone, OptimizeFunction, OptimizeModule } LLVMOptimizations;
		LLVMOptimizations Optimizations;

		virtual ~LLVMState()
		{
			assert(true);
		}

		LLVMState(LLVMOptimizations optimizations)
			: Optimizations(optimizations)
		{
			Context = std::make_unique<llvm::LLVMContext>();
			Builder = std::make_unique<llvm::IRBuilder<>>(*Context);
			Module = std::make_unique<llvm::Module>("AlloyModule", *Context);

			NamedValues.RegisterDefaultTypes(*Context);

			if (optimizations != OptimizeNone)
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
				PB.registerLoopAnalyses(*LAM);
				PB.registerCGSCCAnalyses(*CGAM);
				PB.crossRegisterProxies(*LAM, *FAM, *CGAM, *MAM);

				MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
			}
		}
	};

	bool Generate(ModuleTable& moduleTable, LLVMState& state);
	int Execute(LLVMState& state);
}