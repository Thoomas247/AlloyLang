#pragma once
#include "tokenizer/Token.hpp"

#include <memory>

namespace AlloyCompiler
{
	template <typename T>
	using Ptr = std::unique_ptr<T>;

	template <typename T>
	using Vec = std::vector<std::unique_ptr<T>>;

	constexpr auto ROOT_SYMBOL_TOKEN_ID = std::numeric_limits<TokenID>::max();

	struct Symbol
	{
		Symbol(TokenID tokenID, Symbol* parent)
			: TokenID(tokenID), Parent(parent)
		{}

		TokenID TokenID;

		Symbol* Parent;
		Vec<Symbol> Nested;
	};

	class SymbolTable
	{
	public:
		SymbolTable()
			: m_Root(std::make_unique<Symbol>(ROOT_SYMBOL_TOKEN_ID, nullptr)), m_pCurrentScope(m_Root.get())
		{}

		void PushScope(TokenID tokenID)
		{
			m_pCurrentScope = m_pCurrentScope->Nested.emplace_back(std::make_unique<Symbol>(tokenID, m_pCurrentScope)).get();
		}

		void PopScope()
		{
			m_pCurrentScope = m_pCurrentScope->Parent;
		}

	private:
		Ptr<Symbol> m_Root;

		Symbol* m_pCurrentScope;
	};
}