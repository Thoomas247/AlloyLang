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

		std::unordered_map<std::string_view, NAMED_GROUP_DEFINITION*> GroupDefinitions;
		std::unordered_map<std::string_view, NAMED_SYSTEM_DEFINITION*> SystemDefinitions;
		std::unordered_map<std::string_view, NAMED_QUERY_DEFINITION*> QueryDefinitions;
		std::unordered_map<std::string_view, NAMED_RESOURCE_DEFINITION*> ResourceDefinitions;
		std::unordered_map<std::string_view, NAMED_COMPONENT_DEFINITION*> ComponentDefinitions;
		std::unordered_map<std::string_view, NAMED_TYPE_DEFINITION*> TypeDefinitions;
		std::unordered_map<std::string_view, NAMED_FUNCTION_DEFINITION*> FunctionDefinitions;
		std::unordered_map<std::string_view, EXTERN_FUNCTION_DEFINITION*> ExternDefinitions;

		NamedNodes(size_t allocatorSize)
			: Allocator(allocatorSize)
		{}
	};

	class Parser
	{
	public:
		Parser(const Source& source, const TokenBuffers& tokenBuffers)
			: m_NamedNodes(tokenBuffers.LastTokenID() * 120) // TODO: remove magic number
			, m_CurrentAnnotations()
			, m_Source(source)
			, m_TokenBuffers(tokenBuffers)
			, m_CurrentTokenID(0)
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
		TokenKind kind() const;
		TokenKind peek() const;
		std::string_view value() const;

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
		Result expect(TokenKind* pTokenValue = nullptr, std::string_view* pValueStringView = nullptr);

		template<TokenKind ...Tokens>
		Result expect(std::string_view* pValueStringView, TokenKind* pTokenValue = nullptr);

		Result expectValue(const std::vector<std::string>& values, std::string_view* pValueStringView = nullptr);

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
		const TokenBuffers& m_TokenBuffers;
		TokenID m_CurrentTokenID;
	};

	template<typename... Args>
	constexpr void Parser::logErrorAtCurrentPosition(const std::string& format, Args&&... args)
	{
		const Location& location = m_TokenBuffers.GetLocation(m_CurrentTokenID);
		const size_t tokenSize = m_TokenBuffers.GetValue(m_CurrentTokenID).size();
		const std::string_view line = m_Source.GetLine(location.LineStart);

		Log::Error("({0}:{1}) ERROR:", location.Line, location.Column);
		Log::Error("\t{0}", line);
		Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::string(tokenSize, '~'));
		Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}

	template<typename ...Args>
	constexpr void Parser::logErrorAtPreviousPosition(const std::string& format, Args && ...args)
	{
		ASSERT(m_CurrentTokenID != 0, "Cannot go back to previous position!");

		m_CurrentTokenID--;
		logErrorAtCurrentPosition(format, args...);
		m_CurrentTokenID++;
	}

	template<typename T>
	T* Parser::createNode(const T& node)
	{
		return m_NamedNodes.Allocator.Create<T>(node);
	}

	template<TokenKind ...Tokens>
	Parser::Result Parser::expect(TokenKind* pTokenValue, std::string_view* pValueStringView)
	{
		const bool isExpectedToken = ((m_TokenBuffers.GetKind(m_CurrentTokenID) == Tokens) || ...);

		if (!isExpectedToken)
		{
			const std::vector<TokenKind> tokenKinds = { Tokens... };
			logErrorAtCurrentPosition("Expected token of kind {0}.", tokenKindVectorToString(tokenKinds));
			return UNEXPECTED_KIND;
		}

		if (pTokenValue != nullptr)
		{
			*pTokenValue = m_TokenBuffers.GetKind(m_CurrentTokenID);
		}

		if (pValueStringView != nullptr)
		{
			*pValueStringView = m_TokenBuffers.GetValue(m_CurrentTokenID);
		}

		++m_CurrentTokenID;
		return SUCCESS;
	}

	template<TokenKind ...Tokens>
	Parser::Result Parser::expect(std::string_view* pValueStringView, TokenKind* pTokenValue)
	{
		return expect<Tokens...>(pTokenValue, pValueStringView);
	}
}