#pragma once
#include "ASTNodes.hpp"
#include "Parser.hpp"

#include <memory>
#include <stack>

namespace AlloyCompiler::Parser
{
	using SymbolID = uint32_t;

	constexpr SymbolID ERROR_SYMBOL_ID = std::numeric_limits<SymbolID>::max();

	struct Symbol
	{
		NodeID Node;	// node containing the symbol's identifier

		SymbolID ParentID;
		std::vector<SymbolID> NestedIDs;

		Symbol(NodeID nodeID, SymbolID parentID)
			: Node(nodeID), ParentID(parentID)
		{}
	};

	class SymbolTable
	{
	public:
		SymbolID CreateSymbol(NodeID nodeID)
		{
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
			// look for identifier in the current scope
			for (size_t i = m_ScopeStack.size() - 1; i >= 0; --i)
			{
				for (SymbolID id : m_Symbols[m_ScopeStack[i]].NestedIDs)
				{
					//if (m_NodeData.GetNode(m_Symbols[id].Node) == identifier)
					//{
					//	return m_Symbols[id].Node;
					//}
				}
			}
		}

	private:
		std::deque<SymbolID> m_ScopeStack;
		std::vector<Symbol> m_Symbols;

		NodeDataBuffers& m_NodeData;
	};

	// struct decl is encountered
	//	parse it
	//	look for type identifier (struct name) in the symbol table's current scope
	//		if it exists, error ("Type already defined within this scope! See previous definition:")
	//	create new TypeIdentifier node with type identifier
	//	add it to the symbol table's current scope
	//	create new scope
	//	parse members
	//	pop scope

	// enum decl is encountered
	//	parse it
	//	look for type identifier (enum name) in the symbol table's current scope
	//		if it exists, error ("Type already defined within this scope! See previous definition:")
	//	create new TypeIdentifier node with type identifier
	//	add it to the symbol table's current scope
	//	create new scope
	//	parse members
	//	pop scope

	// fn decl is encountered
	//	parse it
	//  look for argument and return types in the symbol table
	// 		if any doesn't exist, error ("Type must be defined before being used!")
	//	create mangled name (this is the function's identifier, eg. doSomething(args)(ret))
	//	look for mangled name in the symbol table's current scope
	//		if it exists, error ("Function already defined within this scope! See previous definition:")
	//	create new FunctionIdentifier node with mangled name
	//	add it to the symbol table's current scope
	//	create new scope
	//	add argument identifiers to the symbol table's current scope
	//	parse body
	//	pop scope

	// var/const decl is encountered (do not contain their own scope)
	//	parse it
	//	look for type identifier in the symbol table's current scope
	//		if it doesn't exist, error ("Type must be defined before being used!")
	//	look for identifier in the symbol table's current scope
	//		if it exists, error ("Duplicate variable name in this scope! See previous use:")
	//	create new VariableIdentifier node with type and identifier
	//	add it to the symbol table's current scope
}