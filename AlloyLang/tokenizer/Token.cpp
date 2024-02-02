#include "Token.hpp"

namespace AlloyCompiler
{
	static void increment(TokenID& tokenID)
	{
		tokenID = (TokenID)((size_t)tokenID + 1);
	}

	static void decrement(TokenID& tokenID)
	{
		tokenID = (TokenID)((size_t)tokenID - 1);
	}

	TokenBuffers::Iterator::Iterator(const TokenBuffers& tokenBuffers)
		: m_TokenBuffers(tokenBuffers), m_CurrentID((TokenID)0)
	{}

	bool TokenBuffers::Iterator::Next()
	{
		if (m_CurrentID == m_TokenBuffers.LastTokenID())
		{
			return false;
		}

		increment(m_CurrentID);
		return true;
	}

	bool TokenBuffers::Iterator::HasNext() const { return m_CurrentID != m_TokenBuffers.LastTokenID(); }

	void TokenBuffers::Iterator::Previous()
	{
		ASSERT(m_CurrentID != (TokenID)0, "Cannot go back from the first token! This shouldn't happen...");

		decrement(m_CurrentID);
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

	std::string_view TokenBuffers::Iterator::GetLine()
	{
		// find first token of line
		TokenID firstTokenID = m_CurrentID;

		while (firstTokenID != (TokenID)0 && m_TokenBuffers.GetLocation(firstTokenID).Line == m_TokenBuffers.GetLocation(m_CurrentID).Line)
		{
			decrement(firstTokenID);
		}

		increment(firstTokenID);

		// find token after last token of line
		TokenID lastTokenID = m_CurrentID;

		while (lastTokenID != m_TokenBuffers.LastTokenID() && m_TokenBuffers.GetLocation(lastTokenID).Line == m_TokenBuffers.GetLocation(m_CurrentID).Line)
		{
			increment(lastTokenID);
		}

		return std::string_view(m_TokenBuffers.GetValue(firstTokenID).Data(),
			(m_TokenBuffers.GetLocation(lastTokenID).LineStart - m_TokenBuffers.GetLocation(firstTokenID).LineStart) - 1);
	}

	TokenID TokenBuffers::Iterator::GetCurrentID() const
	{
		return m_CurrentID;
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

		return (TokenID)(m_Kinds.size() - 1);
	}

	TokenID TokenBuffers::LastTokenID() const
	{
		return (TokenID)(m_Kinds.size() - 1);
	}

	TokenKind TokenBuffers::GetKind(TokenID id) const
	{
		return m_Kinds[(size_t)id];
	}

	const SmallStringView& TokenBuffers::GetValue(TokenID id) const
	{
		return m_Values[(size_t)id];
	}

	const Location& TokenBuffers::GetLocation(TokenID id) const
	{
		return m_Locations[(size_t)id];
	}

}
