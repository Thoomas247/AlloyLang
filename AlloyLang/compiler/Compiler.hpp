#pragma once
#include <filesystem>
#include <fstream>

#include "module/Module.hpp"

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
		std::unordered_map<std::string, Module> m_Modules;
	};

}