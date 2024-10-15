#pragma once

#include <unordered_set>

#include "../source/Source.hpp"
#include "../token/TokenBuffer.hpp"
#include "nodes/AllNodes.hpp"
#include "NodeAllocator.hpp"
#include "Definition.hpp"
#include "Annotation.hpp"

namespace AlloyCompiler
{
	class NodeBuffer
	{
	public:
		template<typename T>
		using NodeMap = std::unordered_map<std::string, Definition<T>>;

		using ImportedModulesMap = std::unordered_map<std::string_view, std::vector<std::string_view>>;

	public:
		static std::string GetMangledName(const std::string_view& moduleName, const std::string_view& typeName, const std::string_view& functionName)
		{
			//
			// Returns moduleName::typeName@functionName
			//
			std::string result;

			if (!moduleName.empty())
			{
				result = std::string(moduleName) + "::";
			}
			result += std::string(typeName) + "@" + std::string(functionName);
			return result;
		}

		static std::string GetMangledName(const std::string_view& moduleName, Token* pTypeNameToken, Token* pFunctionNameToken)
		{
			return GetMangledName(moduleName,
				pTypeNameToken ? pTypeNameToken->Value : "",
				pFunctionNameToken->Value);
		}

		NodeBuffer(const Source& source, TokenBuffer& tokenBuffer);
		bool Parse();

		const ImportedModulesMap& GetImportedModules() const
		{
			return m_ImportedModules;
		}

		const NodeMap<MACRO_DEFINITION>& GetMacroDefinitions() const
		{
			return m_MacroDefinitions;
		}

		const NodeMap<TYPE_DEFINITION>& GetTypeDefinitions() const
		{
			return m_TypeDefinitions;
		}

		const NodeMap<FUNCTION_DEFINITION>& GetFunctionDefinitions() const
		{
			return m_FunctionDefinitions;
		}

		const NodeMap<VARIABLE_DEFINITION>& GetGlobalVariableDefinitions() const
		{
			return m_GlobalVariableDefinitions;
		}

	private:
		enum Result
		{
			SUCCESS = 0,
			EOF_REACHED,
			UNEXPECTED_KIND,
			UNEXPECTED_VALUE,
			UNKNOWN_ANNOTATION,
			INVALID_ANNOTATION,
			DUPLICATE_ANNOTATION
		};

	private:
		std::string tokenKindVectorToString(const std::vector<TokenKind>& tokens) const;
		std::string stringVectorToString(const std::vector<std::string>& tokens) const;

		template<typename... Args>
		constexpr void logErrorAtCurrentPosition(const std::string& format, Args&&... args);

		template<typename... Args>
		constexpr void logErrorAtPreviousPosition(const std::string& format, Args&&... args);

		template<typename ...Args>
		constexpr void logErrorAtToken(Token* pToken, const std::string& format, Args && ...args);

		template <typename T, typename... Ts>
		T* parse(Ts...) = delete;

		Result getAnnotation();
		Result getImport();

		EXPRESSION* parse_PRIMARY();
		EXPRESSION* parse_POSTFIX();
		EXPRESSION* parse_UNARY();
		EXPRESSION* parse_BINARY();

		template <typename T>
		T* createNode(const T& node);

		bool isEOF() const;
		bool hasNext() const;

		void getNextNonComment();

		Token* token();
		Token* peekToken(size_t offset = 0);

		/// <summary>
		/// Moves onto the next token if current token is not EOF.
		/// </summary>
		Result eat();

		/// <summary>
		/// Moves onto the next token if the current token is one of the expected kinds.
		/// </summary>
		template<TokenKind ...Tokens>
		Result expectKind(Token** ppToken = nullptr);

		Result expectValue(const std::vector<std::string>& values, Token** ppToken = nullptr);

		/// <summary>
		/// Checks that no annotations beyond the given annotations are current.
		/// </summary>
		Result checkAnnotations(const std::unordered_set<std::string_view>& validAnnotations);

		/// <summary>
		/// If annotation exists, removes it and returns true.
		/// If pArgs is not nullptr, fills it in with this annotation's arguments, if any.
		/// </summary>
		bool consumeAnnotation(const std::string_view& name, AnnotationArgs* pArgs = nullptr);

		/// <summary>
		/// Gets the offset of the token after the matching token with the given kind.
		/// For example, with the sequence "(())()", the function would return 4 as that 
		/// is the distance to the token after the closing parenthesis which matches 
		/// the first one.
		/// </summary>
		size_t getOffsetToClosing(TokenKind closing);
		size_t getOffsetToClosing(char closing);

	private:
		const Source& m_Source;
		TokenBuffer& m_TokenBuffer;

		NodeAllocator m_Allocator;

		NodeMap<MACRO_DEFINITION> m_MacroDefinitions;
		NodeMap<TYPE_DEFINITION> m_TypeDefinitions;
		NodeMap<FUNCTION_DEFINITION> m_FunctionDefinitions;
		NodeMap<VARIABLE_DEFINITION> m_GlobalVariableDefinitions;

		std::unordered_set<std::string_view> m_AllSymbolNames;
		ImportedModulesMap m_ImportedModules;

		AnnotationMap m_CurrentAnnotations;
		size_t m_CurrentTokenIndex;
	};

	template<typename... Args>
	constexpr void NodeBuffer::logErrorAtCurrentPosition(const std::string& format, Args&&... args)
	{
		Token* pToken = m_TokenBuffer.GetToken(m_CurrentTokenIndex);

		logErrorAtToken(pToken, format, args...);
	}

	template<typename ...Args>
	constexpr void NodeBuffer::logErrorAtPreviousPosition(const std::string& format, Args&& ...args)
	{
		ASSERT(m_CurrentTokenIndex != 0, "Cannot go back to previous position!");

		Token* pToken = m_TokenBuffer.GetToken(m_CurrentTokenIndex - 1);

		logErrorAtToken(pToken, format, args...);
	}

	template<typename ...Args>
	constexpr void NodeBuffer::logErrorAtToken(Token* pToken, const std::string& format, Args&& ...args)
	{
		const Location& location = pToken->Location;
		const size_t tokenSize = pToken->Value.size();
		const std::string_view line = m_Source.GetLine(location.LineStart);

		Log::Error("({0}:{1}) ERROR:", location.Line, location.Column);
		Log::Error("    {0}", line);
		Log::Error("    {0}{1}", std::string(location.Column - 1, ' '), std::string(tokenSize, '~'));
		Log::Error("    {0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));

	}

	template<typename T>
	T* NodeBuffer::createNode(const T& node)
	{
		return m_Allocator.Create<T>(node);
	}

	template<TokenKind ...Tokens>
	NodeBuffer::Result NodeBuffer::expectKind(Token** ppToken)
	{
		const bool isExpectedToken = ((m_TokenBuffer.GetToken(m_CurrentTokenIndex)->Kind == Tokens) || ...);

		if (!isExpectedToken)
		{
			const std::vector<TokenKind> tokenKinds = { Tokens... };
			logErrorAtCurrentPosition("Expected token of kind {0}.", tokenKindVectorToString(tokenKinds));
			return UNEXPECTED_KIND;
		}

		if (ppToken != nullptr)
		{
			*ppToken = m_TokenBuffer.GetToken(m_CurrentTokenIndex);
		}

		getNextNonComment();

		return SUCCESS;
	}
}