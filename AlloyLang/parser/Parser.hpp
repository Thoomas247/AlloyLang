#pragma once

#include "NodeAllocator.hpp"
#include "Nodes.hpp"
#include "Annotation.hpp"

#include <unordered_set>

namespace AlloyCompiler
{
	struct NamedNodes
	{
		NodeAllocator Allocator;

		std::unordered_map<std::string_view, APPLICATION_DEFINITION*> ApplicationDefinitions;

		std::unordered_map<std::string_view, GROUP_DEFINITION*> GroupDefinitions;
		std::unordered_map<std::string_view, SYSTEM_DEFINITION*> SystemDefinitions;
		std::unordered_map<std::string_view, QUERY_DEFINITION*> QueryDefinitions;
		std::unordered_map<std::string_view, RESOURCE_DEFINITION*> ResourceDefinitions;
		std::unordered_map<std::string_view, COMPONENT_DEFINITION*> ComponentDefinitions;
		std::unordered_map<std::string_view, TYPE_DEFINITION*> TypeDefinitions;
		std::unordered_map<std::string_view, NAMED_FUNCTION_DEFINITION*> FunctionDefinitions;
		std::unordered_map<std::string_view, EXTERN_DEFINITION*> ExternDefinitions;

		NamedNodes(size_t allocatorSize)
			: Allocator(allocatorSize)
		{}
	};

	class Parser
	{
	public:
		Parser(const Source& source, TokenBuffers& tokenBuffers)
			: m_NamedNodes(tokenBuffers.NumTokens() * 120) // TODO: remove magic number
			, m_CurrentAnnotations()
			, m_Source(source)
			, m_TokenBuffers(tokenBuffers)
			, m_CurrentTokenIndex(0)
		{
		}

		NamedNodes Parse();

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

		template <typename T, typename... Ts>
		T* parse(Ts...) = delete;

		Result getAnnotation();

		EXPRESSION* parse_PRIMARY();
		EXPRESSION* parse_POSTFIX();
		EXPRESSION* parse_UNARY();
		EXPRESSION* parse_BINARY();

		template <typename T>
		T* createNode(const T& node);
		template <typename T>
		bool namedNodeExists(const std::string_view& name) const = delete;
		template <typename T>
		void addNamedNode(const std::string_view& name, T* pNode) = delete;

		bool isEOF() const;
		bool hasNext() const;

		Token* token();
		Token* peekToken();

		/// <summary>
		/// Moves onto the next token if current token is not EOF.
		/// </summary>
		Result eat();

		/// <summary>
		/// Moves onto the next token if the current token is one of the expected kinds.
		/// If pTokenValue is not nullptr, it will be set to the kind of the current token.
		/// If pValueView is not nullptr, it will be set to the value of the current token.
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

	private:
		NamedNodes m_NamedNodes;
		AnnotationMap m_CurrentAnnotations;

		const Source& m_Source;
		TokenBuffers& m_TokenBuffers;
		size_t m_CurrentTokenIndex;
	};

	template<typename... Args>
	constexpr void Parser::logErrorAtCurrentPosition(const std::string& format, Args&&... args)
	{
		const Token* pToken = m_TokenBuffers.GetToken(m_CurrentTokenIndex);

		const Location& location = pToken->Location;
		const size_t tokenSize = pToken->Value.size();
		const std::string_view line = m_Source.GetLine(location.LineStart);

		Log::Error("({0}:{1}) ERROR:", location.Line, location.Column);
		Log::Error("\t{0}", line);
		Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::string(tokenSize, '~'));
		Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}

	template<typename ...Args>
	constexpr void Parser::logErrorAtPreviousPosition(const std::string& format, Args && ...args)
	{
		ASSERT(m_CurrentTokenIndex != 0, "Cannot go back to previous position!");

		m_CurrentTokenIndex--;
		logErrorAtCurrentPosition(format, args...);
		m_CurrentTokenIndex++;
	}

	template<typename T>
	T* Parser::createNode(const T& node)
	{
		return m_NamedNodes.Allocator.Create<T>(node);
	}

	template<TokenKind ...Tokens>
	Parser::Result Parser::expectKind(Token** ppToken)
	{
		const bool isExpectedToken = ((m_TokenBuffers.GetToken(m_CurrentTokenIndex)->Kind == Tokens) || ...);

		if (!isExpectedToken)
		{
			const std::vector<TokenKind> tokenKinds = { Tokens... };
			logErrorAtCurrentPosition("Expected token of kind {0}.", tokenKindVectorToString(tokenKinds));
			return UNEXPECTED_KIND;
		}

		if (ppToken != nullptr)
		{
			*ppToken = m_TokenBuffers.GetToken(m_CurrentTokenIndex);
		}

		++m_CurrentTokenIndex;
		return SUCCESS;
	}
}