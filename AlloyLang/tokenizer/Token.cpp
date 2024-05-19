#include "Token.hpp"

namespace AlloyCompiler
{
	void TokenBuffers::AddToken(TokenKind kind, const std::string_view& value, const Location& location)
	{
		m_Tokens.push_back(Token{ .Value = value, .Location = location, .Kind = kind });
	}

	Token* TokenBuffers::GetToken(size_t index)
	{
		ASSERT(index < m_Tokens.size(), "Index is out of range!");

		return &m_Tokens.at(index);
	}

	size_t TokenBuffers::NumTokens() const
	{
		return m_Tokens.size();
	}
}
