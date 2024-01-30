#include "Token.hpp"

namespace AlloyCompiler
{
	TokenBuffers::Iterator::Iterator(const TokenBuffers& tokenBuffers)
		: m_TokenBuffers(tokenBuffers), m_CurrentID(0)
	{}

	bool TokenBuffers::Iterator::Expect(const std::vector<TokenKind>& options)
	{
		if (!Next())
		{
			logErrorAtPosition("Unexpected end of file! Expected {0}.", tokenKindVectorToString(options));
			return false;
		}

		for (const TokenKind option : options)
		{
			if (GetKind() == option)
			{
				return true;
			}
		}

		logErrorAtPosition("Unexpected token '{0}'! Expected {1}.", GetValue().ToStringView(), tokenKindVectorToString(options));
		return false;
	}


	bool TokenBuffers::Iterator::Expect(std::vector<std::string> options)
	{
		if (!Next())
		{
			logErrorAtPosition("Unexpected end of file! Expected {0}.", stringVectorToString(options));
			return false;
		}

		for (const std::string& option : options)
		{
			if (GetValue().ToStringView() == option)
			{
				return true;
			}
		}

		logErrorAtPosition("Unexpected token '{0}'! Expected {1}.", GetValue().ToStringView(), stringVectorToString(options));
		return false;
	}


	bool TokenBuffers::Iterator::Next()
	{
		if (m_CurrentID == m_TokenBuffers.LastTokenID())
		{
			return false;
		}

		++m_CurrentID;
		return true;
	}

	void TokenBuffers::Iterator::Previous()
	{
		ASSERT(m_CurrentID != 0, "Cannot go back from the first token! This shouldn't happen...");

		--m_CurrentID;
	}

	TokenKind TokenBuffers::Iterator::GetKind() const
	{
		return m_TokenBuffers.GetKind(m_CurrentID);
	}

	const SmallStringView& TokenBuffers::Iterator::GetValue() const
	{
		return m_TokenBuffers.GetValue(m_CurrentID);
	}

	const Location& TokenBuffers::Iterator::GetLocation() const
	{
		return m_TokenBuffers.GetLocation(m_CurrentID);
	}

	std::string TokenBuffers::Iterator::tokenKindVectorToString(const std::vector<TokenKind>& tokens)
	{
		if (tokens.empty())
		{
			return "";
		}

		if (tokens.size() == 1)
		{
			return TOKEN_KIND_NAMES.at(tokens[0]);
		}

		std::string result;

		for (size_t i = 0; i < tokens.size() - 1; ++i)
		{
			result += TOKEN_KIND_NAMES.at(tokens[i]) + ", ";
		}

		result += "or " + TOKEN_KIND_NAMES.at(tokens[tokens.size() - 1]);

		return result;
	}

	std::string TokenBuffers::Iterator::stringVectorToString(const std::vector<std::string>& strings)
	{
		if (strings.empty())
		{
			return "";
		}

		if (strings.size() == 1)
		{
			return strings[0];
		}

		std::string result;

		for (size_t i = 0; i < strings.size() - 1; ++i)
		{
			result += strings[i] + ", ";
		}

		result += "or " + strings[strings.size() - 1];

		return result;
	}

	std::string_view TokenBuffers::Iterator::getLine()
	{
		// find first token of line
		TokenID firstTokenID = m_CurrentID;

		while (firstTokenID != 0 && m_TokenBuffers.GetLocation(firstTokenID).Line == m_TokenBuffers.GetLocation(m_CurrentID).Line)
		{
			--firstTokenID;
		}

		firstTokenID++;

		// find token after last token of line
		TokenID lastTokenID = m_CurrentID;

		while (lastTokenID != m_TokenBuffers.LastTokenID() && m_TokenBuffers.GetLocation(lastTokenID).Line == m_TokenBuffers.GetLocation(m_CurrentID).Line)
		{
			++lastTokenID;
		}

		return std::string_view(m_TokenBuffers.GetValue(firstTokenID).Data(),
			m_TokenBuffers.GetLocation(lastTokenID).LineStart - m_TokenBuffers.GetLocation(firstTokenID).LineStart);
	}

	TokenBuffers::Iterator TokenBuffers::GetIterator() const
	{
		return Iterator(*this);
	}

	TokenID TokenBuffers::AddToken(TokenKind kind, const SmallStringView& value, const Location& location)
	{
		m_Kinds.push_back(kind);
		m_Values.push_back(value);
		m_Locations.push_back(location);

		return (uint32_t)m_Kinds.size() - 1;
	}

	TokenID TokenBuffers::LastTokenID() const
	{
		return (TokenID)m_Kinds.size() - 1;
	}

	TokenKind TokenBuffers::GetKind(TokenID id) const
	{
		return m_Kinds[id];
	}

	const SmallStringView& TokenBuffers::GetValue(TokenID id) const
	{
		return m_Values[id];
	}

	const Location& TokenBuffers::GetLocation(TokenID id) const
	{
		return m_Locations[id];
	}

}
