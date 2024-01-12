#pragma once
#include <string_view>
#include <vector>

#include "Token.hpp"

namespace AlloyCompiler::Tokenizer
{
	class TokenDataBuffers
	{
	public:
		TokenDataBuffers(const std::string_view& source)
			: m_FullSourceView(source)
		{
			// this is a very rough estimate which assumes that every token is 2 characters long
			// this probably allocates more than necessary, but it's better than not allocating enough
			// cuts down about 1/3 of the tokenization time
			const size_t estTokenCount = source.size() / 2;

			m_Tokens.reserve(estTokenCount);
			m_Locations.reserve(estTokenCount);
			m_SourceViews.reserve(estTokenCount);
		}

		void AddToken(const Token& token, const Location& location, const SourceView& sourceView)
		{
			m_Tokens.emplace_back(token);
			m_Locations.emplace_back(location);
			m_SourceViews.emplace_back(sourceView);
		}

		const Token& GetToken(TokenID tokenID) const { return m_Tokens[tokenID]; }
		const Location& GetLocation(TokenID tokenID) const { return m_Locations[tokenID]; }
		const SourceView& GetSourceView(TokenID tokenID) const { return m_SourceViews[tokenID]; }

		size_t GetTokenCount() const { return m_Tokens.size(); }

		SourceView GetSourceView() const { return m_FullSourceView; }

	private:
		SourceView m_FullSourceView;

		std::vector<Token> m_Tokens;
		std::vector<Location> m_Locations;
		std::vector<SourceView> m_SourceViews;
	};

	TokenDataBuffers Tokenize(const std::string_view& source);
}