#pragma once
#include <filesystem>
#include <fstream>

#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"

namespace AlloyCompiler
{
	namespace fs = std::filesystem;

	constexpr auto FILE_EXTENSION = ".alloy";

	class Compiler
	{
	public:
		Compiler(const std::string& mainFilePath)
			: m_MainFilePath(fs::absolute(mainFilePath))
		{}

		void Compile()
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
				m_Modules.insert({ getModuleName(path), std::move(parseModule(path)) });
			}
		}

	private:
		Module parseModule(const fs::path& path)
		{
			std::ifstream fileStream(path);

			std::stringstream stringStream;
			stringStream << fileStream.rdbuf();

			fileStream.close();

			std::string sourceString = stringStream.str() + '\n';	// add an extra character so that the tokenizer works properly

			Source source(sourceString);

			Tokenizer tokenizer(source);
			TokenBuffers tokenBuffers = tokenizer.Tokenize();

			Parser parser(source, tokenBuffers);

			m_SourceStrings.push_back(std::move(sourceString));	// keep the string alive until the end of the compilation

			return std::move(parser.Parse());
		}

		std::string getModuleName(const fs::path& path)
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

	private:
		fs::path m_MainFilePath;
		std::vector<std::string> m_SourceStrings;
		std::unordered_map<std::string, Module> m_Modules;
	};

}