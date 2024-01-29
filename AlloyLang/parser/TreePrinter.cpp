#include "TreePrinter.hpp"

#include "log/Log.hpp"

#define NAME_OF_NODE(x) tokenBuffers.GetSourceView(nodeBuffers.GetNode(x.IdentifierID).Identifier.Token)

namespace AlloyCompiler::Parser
{
	constexpr auto INDENT_SIZE = 2;

	inline static void printNode(const Tokenizer::TokenDataBuffers& tokenBuffers, const NodeDataBuffers& nodeBuffers, NodeID nodeID, size_t indent)
	{
		std::string indentStr = std::string(indent * INDENT_SIZE, ' ');

		if (nodeID == ERROR_NODE_ID)
		{
			Log::Print("{0}ERROR_NODE_ID", indentStr);
			return;
		}

		const auto& currentNode = nodeBuffers.GetNode(nodeID);

		switch (currentNode.Kind)
		{
		case NodeKind::TypeIdentifier:
			Log::Print("{0}TypeIdentifier: {1}", indentStr, NAME_OF_NODE(currentNode.TypeIdentifier));
			break;


		case NodeKind::IntegerLiteral:
			Log::Print("{0}IntegerLiteral: {1}", indentStr, currentNode.IntegerLiteral.Value);
			break;

		case NodeKind::FloatLiteral:
			Log::Print("{0}FloatLiteral: {1}", indentStr, currentNode.FloatLiteral.Value);
			break;

		case NodeKind::BooleanLiteral:
			Log::Print("{0}BooleanLiteral: {1}", indentStr, currentNode.BooleanLiteral.Value);
			break;

		case NodeKind::StringLiteral:
			Log::Print("{0}StringLiteral: {1}", indentStr, currentNode.StringLiteral.Value);
			break;

		case NodeKind::CharacterLiteral:
			Log::Print("{0}CharacterLiteral: {1}", indentStr, currentNode.CharacterLiteral.Value);
			break;


		case NodeKind::Binary:
			Log::Print("{0}Binary: {1}", indentStr, (size_t)currentNode.Binary.Op);
			printNode(tokenBuffers, nodeBuffers, currentNode.Binary.Left, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.Binary.Right, indent + 1);
			break;

		case NodeKind::Unary:
			Log::Print("{0}Unary: {1}", indentStr, (size_t)currentNode.Unary.Op);
			printNode(tokenBuffers, nodeBuffers, currentNode.Unary.Operand, indent + 1);
			break;

		case NodeKind::AssignmentExpression:
			Log::Print("{0}AssignmentExpression: {1} {2}", indentStr, NAME_OF_NODE(currentNode.AssignmentExpression), (size_t)currentNode.AssignmentExpression.Op);
			printNode(tokenBuffers, nodeBuffers, currentNode.AssignmentExpression.Value, indent + 1);
			break;

		case NodeKind::MemoryAccess:
			Log::Print("{0}MemoryAccess: {1}", indentStr, NAME_OF_NODE(currentNode.MemoryAccess));
			break;

		case NodeKind::FunctionCall:
			Log::Print("{0}FunctionCall: {1}", indentStr, NAME_OF_NODE(currentNode.FunctionCall));
			for (const auto& arg : currentNode.FunctionCall.Arguments.List)
			{
				printNode(tokenBuffers, nodeBuffers, arg, indent + 1);
			}
			break;

		case NodeKind::Enclosed:
			Log::Print("{0}Enclosed:", indentStr);
			for (const auto& arg : currentNode.Enclosed.TupleExpressions.List)
			{
				printNode(tokenBuffers, nodeBuffers, arg, indent + 1);
			}
			break;

		case NodeKind::AssignmentStatement:
			Log::Print("{0}AssignmentStatement:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.AssignmentStatement.Assignment, indent + 1);
			break;

		case NodeKind::Return:
			Log::Print("{0}Return:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.Return.Expression, indent + 1);
			break;

		case NodeKind::StatementBlock:
			Log::Print("{0}StatementBlock:", indentStr);
			for (const auto& statement : currentNode.StatementBlock.Statements.List)
			{
				printNode(tokenBuffers, nodeBuffers, statement, indent + 1);
			}
			break;

		case NodeKind::For:
			Log::Print("{0}For:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.For.Init, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.For.Condition, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.For.Increment, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.For.Body, indent + 1);
			break;

		case NodeKind::While:
			Log::Print("{0}While:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.While.Condition, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.While.Body, indent + 1);
			break;

		case NodeKind::If:
			Log::Print("{0}If:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.If.Condition, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.If.Body, indent + 1);
			break;

		case NodeKind::Match:
			Log::Print("{0}Match:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.Match.Expression, indent + 1);
			for (const auto& c : currentNode.Match.Cases.List)
			{
				printNode(tokenBuffers, nodeBuffers, c, indent + 1);
				printNode(tokenBuffers, nodeBuffers, c, indent + 1);
			}
			break;

		case NodeKind::VariableTypeDeclaration:
			Log::Print("{0}VariableTypeDeclaration:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.VariableTypeDeclaration.Type, indent + 1);
			break;

		case NodeKind::ConstantTypeDeclaration:
			Log::Print("{0}ConstantTypeDeclaration:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.ConstantTypeDeclaration.Type, indent + 1);
			break;

		case NodeKind::ValueTypeDeclaration:
			Log::Print("{0}ValueTypeDeclaration:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.ValueTypeDeclaration.Type, indent + 1);
			break;

		case NodeKind::TupleTypeDeclaration:
			Log::Print("{0}TupleTypeDeclaration:", indentStr);
			for (const auto& t : currentNode.TupleTypeDeclaration.Types.List)
			{
				printNode(tokenBuffers, nodeBuffers, t, indent + 1);
			}
			break;

		case NodeKind::VariableDeclaration:
			Log::Print("{0}VariableDeclaration: {1}", indentStr, NAME_OF_NODE(currentNode.VariableDeclaration));
			printNode(tokenBuffers, nodeBuffers, currentNode.VariableDeclaration.Type, indent + 1);
			break;

		case NodeKind::ConstantDeclaration:
			Log::Print("{0}ConstantDeclaration: {1}", indentStr, NAME_OF_NODE(currentNode.ConstantDeclaration));
			printNode(tokenBuffers, nodeBuffers, currentNode.ConstantDeclaration.Type, indent + 1);
			break;

		case NodeKind::VariableDefinition:
			Log::Print("{0}VariableDefinition:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.VariableDefinition.Declaration, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.VariableDefinition.Value, indent + 1);
			break;

		case NodeKind::ConstantDefinition:
			Log::Print("{0}ConstantDefinition:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.ConstantDefinition.Declaration, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.ConstantDefinition.Value, indent + 1);
			break;

		case NodeKind::StructDefinition:
			Log::Print("{0}StructDefinition: {1}", indentStr, NAME_OF_NODE(currentNode.StructDefinition));
			for (const auto& member : currentNode.StructDefinition.Members.List)
			{
				printNode(tokenBuffers, nodeBuffers, member, indent + 1);
			}
			break;

		case NodeKind::EnumMember:
			Log::Print("{0}EnumMember: {1}", indentStr, NAME_OF_NODE(currentNode.EnumMember));
			break;

		case NodeKind::EnumDefinition:
			Log::Print("{0}EnumDefinition: {1}", indentStr, NAME_OF_NODE(currentNode.EnumDefinition));
			for (const auto& member : currentNode.EnumDefinition.Members.List)
			{
				printNode(tokenBuffers, nodeBuffers, member, indent + 1);
			}
			break;

		case NodeKind::FunctionDefinition:
			Log::Print("{0}FunctionDefinition: {1}", indentStr, NAME_OF_NODE(currentNode.FunctionDefinition));
			for (const auto& param : currentNode.FunctionDefinition.Parameters.List)
			{
				printNode(tokenBuffers, nodeBuffers, param, indent + 1);
			}
			printNode(tokenBuffers, nodeBuffers, currentNode.FunctionDefinition.ReturnType, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.FunctionDefinition.Body, indent + 1);
			break;

		case NodeKind::QualifiedDefinition:
			Log::Print("{0}QualifiedDefinition: {1}", indentStr, (size_t)currentNode.QualifiedDefinition.Visibility);
			printNode(tokenBuffers, nodeBuffers, currentNode.QualifiedDefinition.Definition, indent + 1);
			break;

		case NodeKind::Module:
			Log::Print("{0}Module:", indentStr);
			for (const auto& def : currentNode.Module.QualifiedDefinitions.List)
			{
				printNode(tokenBuffers, nodeBuffers, def, indent + 1);
			}
			break;

		case NodeKind::Program:
			Log::Print("{0}Program:", indentStr);
			for (const auto& module : currentNode.Program.Modules.List)
			{
				printNode(tokenBuffers, nodeBuffers, module, indent + 1);
			}
			break;

		default:
			Log::Print("{0}Unknown node kind: {1}", indentStr, (size_t)currentNode.Kind);
			break;

		}
	}

	void PrintTree(const Tokenizer::TokenDataBuffers& tokenBuffers, const NodeDataBuffers& nodeBuffers)
	{
		printNode(tokenBuffers, nodeBuffers, nodeBuffers.GetRootNodeID(), 0);
	}
}


