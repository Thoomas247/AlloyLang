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


	TokenID TokenBuffers::AddToken(TokenKind kind, const std::string_view& value, const Location& location)
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

	const std::string_view& TokenBuffers::GetValue(TokenID id) const
	{
		ASSERT((size_t)id < m_Values.size(), "Invalid ID!");

		return m_Values[(size_t)id];
	}

	const Location& TokenBuffers::GetLocation(TokenID id) const
	{
		return m_Locations[(size_t)id];
	}

}
