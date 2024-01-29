#pragma once
#include "Symbol.hpp"
#include "parser/Parser.hpp"

namespace AlloyCompiler::Symbols
{
	class SymbolTable
	{
	public:
		SymbolTable(Parser::NodeDataBuffers& nodeData)
			: m_NodeData(nodeData)
		{}

		SymbolID CreateSymbol(NodeID nodeID)
		{
			ASSERT(m_NodeData.GetNode(nodeID).Kind == NodeKind::Identifier, "Node must be an identifier!");

			SymbolID id = (SymbolID)m_Symbols.size();

			if (m_ScopeStack.empty())
			{
				m_Symbols.emplace_back(nodeID, ERROR_SYMBOL_ID);
			}
			else
			{
				m_Symbols[m_ScopeStack.back()].NestedIDs.push_back(id);
				m_Symbols.emplace_back(nodeID, m_ScopeStack.back());
			}

			return id;
		}

		void PushScope(SymbolID symbolID)
		{
			ASSERT(symbolID < m_Symbols.size(), "Invalid symbol ID!");

			m_ScopeStack.push_back(symbolID);
		}

		void PopScope()
		{
			ASSERT(!m_ScopeStack.empty(), "No scope to pop!");

			m_ScopeStack.pop_back();
		}

		NodeID GetSymbolNode(const std::string_view& identifier) const
		{
			// look for identifier starting with the current scope
			for (int i = (int)m_ScopeStack.size() - 1; i >= 0; --i)
			{
				for (SymbolID id : m_Symbols[m_ScopeStack[i]].NestedIDs)
				{
					// due to the assert in CreateSymbol, we know that all nodes in the symbol table are identifiers
					if (m_NodeData.GetNode(m_Symbols[id].Node).Identifier.Name == identifier)
					{
						return m_Symbols[id].Node;
					}
				}
			}

			return ERROR_NODE_ID;
		}

		bool IsSymbolInScope(const std::string_view& identifier) const
		{
			for (SymbolID id : m_Symbols.back().NestedIDs)
			{
				// due to the assert in CreateSymbol, we know that all nodes in the symbol table are identifiers
				if (m_NodeData.GetNode(m_Symbols[id].Node).Identifier.Name == identifier)
				{
					return true;
				}
			}

			return false;
		}

	private:
		std::deque<SymbolID> m_ScopeStack;
		std::vector<Symbol> m_Symbols;

		Parser::NodeDataBuffers& m_NodeData;
	};

	SymbolTable Resolve(Parser::NodeDataBuffers& nodeBuffers);
}