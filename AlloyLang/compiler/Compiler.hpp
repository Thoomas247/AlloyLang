#pragma once
#include <filesystem>
#include <fstream>

#include "module/Module.hpp"
#include "codegen/CodeGenerator.hpp"

namespace AlloyCompiler
{
	namespace fs = std::filesystem;

	constexpr auto FILE_EXTENSION = ".alloy";

	class Compiler
	{
	public:
		Compiler(const std::string& mainFilePath, LLVMState::LLVMOptimizations optimize = LLVMState::OptimizeModule);
		void Compile();
		int Execute();

	private:
		std::string getModuleName(const fs::path& path);

	private:
		fs::path m_MainFilePath;
		std::unordered_map<std::string, Module> m_Modules;
		LLVMState m_LLVMState;
	};

}