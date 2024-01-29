#include "Symbols.hpp"

using namespace AlloyCompiler::Parser;

namespace AlloyCompiler::Symbols
{
	struct PendingSymbol
	{
		NodeID Node;
		SymbolID Parent;
	};

	template <typename... Args>
	inline constexpr void logError(Token& iter, const std::string& format, Args&&... args)
	{
		Log::Error("Error at location ({0} : {1}):", iter.CurrentLocation().Line, iter.CurrentLocation().Column);
		Log::Error("\t{0}", iter.GetLine());
		Log::Error("\t{0}^", std::string(iter.CurrentLocation().Column - 1, ' '));
		Log::Error("\t{0}{1}", std::string(iter.CurrentLocation().Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}

	inline static void resolveModule(NodeDataBuffers& nodeBuffers, SymbolTable& symbolTable, NodeID moduleID)
	{
		auto& moduleNode = nodeBuffers.GetNode(moduleID);

		// assert that our module node is a module
		ASSERT(moduleNode.Kind == NodeKind::Module, "Module node must be a module!");

		// TODO: create a symbol for the module

		for (NodeID qualifiedDefinitionID : moduleNode.Module.QualifiedDefinitions.List)
		{
			auto& qualifiedDefinitionNode = nodeBuffers.GetNode(qualifiedDefinitionID);

			// assert that our qualified definition node is a qualified definition
			ASSERT(qualifiedDefinitionNode.Kind == NodeKind::QualifiedDefinition, "Qualified definition node must be a qualified definition!");
		}
	}

	SymbolTable Resolve(NodeDataBuffers& nodeBuffers)
	{
		auto& rootNode = nodeBuffers.GetNode(nodeBuffers.GetRootNodeID());

		// assert that our root node is a program
		ASSERT(rootNode.Kind == NodeKind::Program, "Root node must be the program!");

		SymbolTable symbolTable(nodeBuffers);
		std::vector<PendingSymbol> pendingSymbols;

		// TODO: loop through all modules when we have more than one
		resolveModule(nodeBuffers, symbolTable, rootNode.Program.Modules.List[0]);

		return symbolTable;
	}
}
