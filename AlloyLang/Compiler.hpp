#pragma once
#include <filesystem>
#include <fstream>

#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
#include "codegen/CodeGenerator.hpp"

namespace AlloyCompiler
{
	namespace fs = std::filesystem;

	constexpr auto FILE_EXTENSION = ".alloy";

	class Compiler
	{
	public:
		Compiler(const std::string& mainFilePath);
		void Compile();

	private:
		Module parseModule(const fs::path& path);
		std::string getModuleName(const fs::path& path);

	private:
		fs::path m_MainFilePath;
		std::vector<std::string> m_SourceStrings;
		std::unordered_map<std::string, Module> m_Modules;
	};

}