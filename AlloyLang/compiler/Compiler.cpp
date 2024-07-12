#include "Compiler.hpp"
#include "ModuleTable.hpp"
#include "codegen/CodeGenerator.hpp"

namespace AlloyCompiler
{
	Compiler::Compiler(const std::string& mainFilePath)
		: m_MainFilePath(fs::absolute(mainFilePath))
	{}

	void Compiler::Compile()
	{
		if (!fs::exists(m_MainFilePath))
		{
			Log::Error("Could not find main file '{0}'.", m_MainFilePath.string());
			return;
		}

		if (m_MainFilePath.extension() != FILE_EXTENSION)
		{
			Log::Error("Main file '{0}' must have the correct extension, '{1}'.", m_MainFilePath.string(), FILE_EXTENSION);
			return;
		}

		std::stack<fs::path> pathsToSearch;
		pathsToSearch.push(m_MainFilePath.parent_path());

		std::vector<fs::path> pathsToCompile;

		while (!pathsToSearch.empty())
		{
			fs::path currentPath = pathsToSearch.top();
			pathsToSearch.pop();

			for (const fs::path& path : fs::directory_iterator(currentPath))
			{
				if (fs::is_directory(path))
				{
					pathsToSearch.push(path);
				}
				else if (path.extension() == FILE_EXTENSION)
				{
					pathsToCompile.push_back(path);
				}
			}
		}

		for (const fs::path& path : pathsToCompile)
		{
			m_Modules.emplace(getModuleName(path), path);
		}

		for (auto& [name, module] : m_Modules)
		{
			if (!module.Generate())
			{
				Log::Error("Failed to compile module '{0}'. See output for errors.", name);
				return;
			}
		}

		ModuleTable moduleTable(m_Modules, getModuleName(m_MainFilePath));

		LLVMState llvmState(true);
		Generate(moduleTable, llvmState);
	}

	int Compiler::Compile(const std::string& sourceString, bool execute)
	{
		m_Modules.emplace(".", "");
		m_Modules.at(".").Generate(sourceString);

		ModuleTable moduleTable(m_Modules, getModuleName(m_MainFilePath));

		LLVMState llvmState(true);
		Generate(moduleTable, llvmState);

		if (execute) {
			return Execute(llvmState);
		}
		else {
			return 0;
		}
	}

	std::string Compiler::getModuleName(const fs::path& path)
	{
		fs::path relativePath = fs::relative(path, m_MainFilePath.parent_path());

		std::string pathString = relativePath.string();
		if (pathString.contains(FILE_EXTENSION))
		{
			pathString.resize(pathString.size() - std::string_view(FILE_EXTENSION).size());

			for (size_t i = 0; i < pathString.size(); i++)
			{
				if (pathString[i] == '/' || pathString[i] == '\\')
				{
					pathString[i] = ':';
					pathString.insert(i + 1, 1, ':');
				}
			}
		}
		return pathString;
	}
}
