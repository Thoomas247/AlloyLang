#include "Compiler.hpp"

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
			m_Modules.insert({ getModuleName(path), Module(path) });
		}

		for (auto& [name, module] : m_Modules)
		{
			module.Generate();
		}
	}

	std::string Compiler::getModuleName(const fs::path& path)
	{
		fs::path relativePath = fs::relative(path, m_MainFilePath.parent_path());

		std::string pathString = relativePath.string();
		pathString.resize(pathString.size() - std::string_view(FILE_EXTENSION).size());

		for (size_t i = 0; i < pathString.size(); i++)
		{
			if (pathString[i] == '/' || pathString[i] == '\\')
			{
				pathString[i] = ':';
				pathString.insert(i + 1, 1, ':');
			}
		}

		return pathString;
	}
}
