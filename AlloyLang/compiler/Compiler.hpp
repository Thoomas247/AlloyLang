#pragma once
#include <filesystem>
#include <fstream>

namespace AlloyCompiler
{
	namespace fs = std::filesystem;

	constexpr auto FILE_EXTENSION = ".alloy";

	class Compiler
	{
	public:
		Compiler(const std::string& mainFilePath, LLVMState::LLVMOptimizations optimize = LLVMState::OptimizeModule);
		virtual ~Compiler();

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