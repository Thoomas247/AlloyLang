#include "TreePrinter.hpp"

#include "log/Log.hpp"

#define NAME_OF_NODE(x) buffers.GetNode(x.IdentifierID).Identifier.Name

namespace AlloyCompiler::Parser
{
	constexpr auto INDENT_SIZE = 2;

	inline static void printNode(const NodeDataBuffers& buffers, NodeID nodeID, size_t indent)
	{
		std::string indentStr = std::string(indent * INDENT_SIZE, ' ');

		if (nodeID == ERROR_NODE_ID)
		{
			Log::Print("{0}ERROR_NODE_ID", indentStr);
			return;
		}

		const auto& currentNode = buffers.GetNode(nodeID);

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
			printNode(buffers, currentNode.Binary.Left, indent + 1);
			printNode(buffers, currentNode.Binary.Right, indent + 1);
			break;

		case NodeKind::Unary:
			Log::Print("{0}Unary: {1}", indentStr, (size_t)currentNode.Unary.Op);
			printNode(buffers, currentNode.Unary.Operand, indent + 1);
			break;

		case NodeKind::AssignmentExpression:
			Log::Print("{0}AssignmentExpression: {1} {2}", indentStr, NAME_OF_NODE(currentNode.AssignmentExpression), (size_t)currentNode.AssignmentExpression.Op);
			printNode(buffers, currentNode.AssignmentExpression.Value, indent + 1);
			break;

		case NodeKind::MemoryAccess:
			Log::Print("{0}MemoryAccess: {1}", indentStr, NAME_OF_NODE(currentNode.MemoryAccess));
			break;

		case NodeKind::FunctionCall:
			Log::Print("{0}FunctionCall: {1}", indentStr, NAME_OF_NODE(currentNode.FunctionCall));
			for (const auto& arg : currentNode.FunctionCall.Arguments.List)
			{
				printNode(buffers, arg, indent + 1);
			}
			break;

		case NodeKind::Enclosed:
			Log::Print("{0}Enclosed:", indentStr);
			for (const auto& arg : currentNode.Enclosed.TupleExpressions.List)
			{
				printNode(buffers, arg, indent + 1);
			}
			break;

		case NodeKind::AssignmentStatement:
			Log::Print("{0}AssignmentStatement:", indentStr);
			printNode(buffers, currentNode.AssignmentStatement.Assignment, indent + 1);
			break;

		case NodeKind::Return:
			Log::Print("{0}Return:", indentStr);
			printNode(buffers, currentNode.Return.Expression, indent + 1);
			break;

		case NodeKind::StatementBlock:
			Log::Print("{0}StatementBlock:", indentStr);
			for (const auto& statement : currentNode.StatementBlock.Statements.List)
			{
				printNode(buffers, statement, indent + 1);
			}
			break;

		case NodeKind::For:
			Log::Print("{0}For:", indentStr);
			printNode(buffers, currentNode.For.Init, indent + 1);
			printNode(buffers, currentNode.For.Condition, indent + 1);
			printNode(buffers, currentNode.For.Increment, indent + 1);
			printNode(buffers, currentNode.For.Body, indent + 1);
			break;

		case NodeKind::While:
			Log::Print("{0}While:", indentStr);
			printNode(buffers, currentNode.While.Condition, indent + 1);
			printNode(buffers, currentNode.While.Body, indent + 1);
			break;

		case NodeKind::If:
			Log::Print("{0}If:", indentStr);
			printNode(buffers, currentNode.If.Condition, indent + 1);
			printNode(buffers, currentNode.If.Body, indent + 1);
			break;

		case NodeKind::Match:
			Log::Print("{0}Match:", indentStr);
			printNode(buffers, currentNode.Match.Expression, indent + 1);
			for (const auto& c : currentNode.Match.Cases.List)
			{
				printNode(buffers, c, indent + 1);
				printNode(buffers, c, indent + 1);
			}
			break;

		case NodeKind::VariableTypeDeclaration:
			Log::Print("{0}VariableTypeDeclaration:", indentStr);
			printNode(buffers, currentNode.VariableTypeDeclaration.Type, indent + 1);
			break;

		case NodeKind::ConstantTypeDeclaration:
			Log::Print("{0}ConstantTypeDeclaration:", indentStr);
			printNode(buffers, currentNode.ConstantTypeDeclaration.Type, indent + 1);
			break;

		case NodeKind::ValueTypeDeclaration:
			Log::Print("{0}ValueTypeDeclaration:", indentStr);
			printNode(buffers, currentNode.ValueTypeDeclaration.Type, indent + 1);
			break;

		case NodeKind::TupleTypeDeclaration:
			Log::Print("{0}TupleTypeDeclaration:", indentStr);
			for (const auto& t : currentNode.TupleTypeDeclaration.Types.List)
			{
				printNode(buffers, t, indent + 1);
			}
			break;

		case NodeKind::VariableDeclaration:
			Log::Print("{0}VariableDeclaration: {1}", indentStr, NAME_OF_NODE(currentNode.VariableDeclaration));
			printNode(buffers, currentNode.VariableDeclaration.Type, indent + 1);
			break;

		case NodeKind::ConstantDeclaration:
			Log::Print("{0}ConstantDeclaration: {1}", indentStr, NAME_OF_NODE(currentNode.ConstantDeclaration));
			printNode(buffers, currentNode.ConstantDeclaration.Type, indent + 1);
			break;

		case NodeKind::VariableDefinition:
			Log::Print("{0}VariableDefinition:", indentStr);
			printNode(buffers, currentNode.VariableDefinition.Declaration, indent + 1);
			printNode(buffers, currentNode.VariableDefinition.Value, indent + 1);
			break;

		case NodeKind::ConstantDefinition:
			Log::Print("{0}ConstantDefinition:", indentStr);
			printNode(buffers, currentNode.ConstantDefinition.Declaration, indent + 1);
			printNode(buffers, currentNode.ConstantDefinition.Value, indent + 1);
			break;

		case NodeKind::StructDefinition:
			Log::Print("{0}StructDefinition: {1}", indentStr, NAME_OF_NODE(currentNode.StructDefinition));
			for (const auto& member : currentNode.StructDefinition.Members.List)
			{
				printNode(buffers, member, indent + 1);
			}
			break;

		case NodeKind::EnumMember:
			Log::Print("{0}EnumMember: {1}", indentStr, NAME_OF_NODE(currentNode.EnumMember));
			break;

		case NodeKind::EnumDefinition:
			Log::Print("{0}EnumDefinition: {1}", indentStr, NAME_OF_NODE(currentNode.EnumDefinition));
			for (const auto& member : currentNode.EnumDefinition.Members.List)
			{
				printNode(buffers, member, indent + 1);
			}
			break;

		case NodeKind::FunctionDefinition:
			Log::Print("{0}FunctionDefinition: {1}", indentStr, NAME_OF_NODE(currentNode.FunctionDefinition));
			for (const auto& param : currentNode.FunctionDefinition.Parameters.List)
			{
				printNode(buffers, param, indent + 1);
			}
			printNode(buffers, currentNode.FunctionDefinition.ReturnType, indent + 1);
			printNode(buffers, currentNode.FunctionDefinition.Body, indent + 1);
			break;

		case NodeKind::QualifiedDefinition:
			Log::Print("{0}QualifiedDefinition: {1}", indentStr, (size_t)currentNode.QualifiedDefinition.Visibility);
			printNode(buffers, currentNode.QualifiedDefinition.Definition, indent + 1);
			break;

		case NodeKind::Module:
			Log::Print("{0}Module:", indentStr);
			for (const auto& def : currentNode.Module.QualifiedDefinitions.List)
			{
				printNode(buffers, def, indent + 1);
			}
			break;

		case NodeKind::Program:
			Log::Print("{0}Program:", indentStr);
			for (const auto& module : currentNode.Program.Modules.List)
			{
				printNode(buffers, module, indent + 1);
			}
			break;

		default:
			Log::Print("{0}Unknown node kind: {1}", indentStr, (size_t)currentNode.Kind);
			break;

		}
	}

	void PrintTree(const NodeDataBuffers& buffers)
	{
		printNode(buffers, buffers.GetRootNodeID(), 0);
	}
}


