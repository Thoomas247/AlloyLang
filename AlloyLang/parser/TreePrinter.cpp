#include "TreePrinter.hpp"

#include "../log/Log.hpp"

namespace AlloyCompiler
{
	constexpr auto INDENT_SIZE = 2;

	inline static void printNode(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID, size_t indent)
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
		case NodeKind::LITERAL:
		{
			const auto& value = tokenBuffers.GetValue(currentNode.Literal.InfoTokenID).ToStringView();
			Log::Print("{0}LITERAL: {1}", indentStr, value);
			break;
		}

		case NodeKind::IDENTIFIER:
		{
			const auto& value = tokenBuffers.GetValue(currentNode.Identifier.IdentifierTokenID).ToStringView();
			Log::Print("{0}IDENTIFIER: {1}", indentStr, value);
			break;
		}

		case NodeKind::TYPE_IDENTIFIER:
		{
			const auto& identifierNode = nodeBuffers.GetNode(currentNode.TypeIdentifier.IdentifierID).Identifier;
			const auto& value = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();
			Log::Print("{0}TYPE_IDENTIFIER: {1}", indentStr, value);
			break;
		}

		case NodeKind::TYPE_DECLARATION:
		{
			Log::Print("{0}TYPE_DECLARATION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.TypeDeclaration.TypeIdentifierID, indent + 1);
			break;
		}

		case NodeKind::VALUE_DECLARATION:
		{
			Log::Print("{0}VALUE_DECLARATION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.ValueDeclaration.TypeIdentifierID, indent + 1);
			break;
		}

		case NodeKind::FUNCTION_CALL_EXPRESSION:
		{
			Log::Print("{0}FUNCTION_CALL_EXPRESSION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.FunctionCallExpression.IdentifierID, indent + 1);
			break;
		}

		case NodeKind::ENCLOSED_EXPRESSION:
		{
			Log::Print("{0}ENCLOSED_EXPRESSION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.EnclosedExpression.ExpressionID, indent + 1);
			break;
		}

		case NodeKind::BINARY_EXPRESSION:
		{
			Log::Print("{0}BINARY_EXPRESSION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.BinaryExpression.LeftID, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.BinaryExpression.RightID, indent + 1);
			break;
		}

		case NodeKind::UNARY_EXPRESSION:
		{
			Log::Print("{0}UNARY_EXPRESSION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.UnaryExpression.OperandID, indent + 1);
			break;
		}

		case NodeKind::ASSIGNMENT_EXPRESSION:
		{
			Log::Print("{0}ASSIGNMENT_EXPRESSION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.AssignmentExpression.IdentifierID, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.AssignmentExpression.ValueID, indent + 1);
			break;
		}

		case NodeKind::ASSIGNMENT_STATEMENT:
		{
			Log::Print("{0}ASSIGNMENT_STATEMENT:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.AssignmentStatement.AssignmentExpressionID, indent + 1);
			break;
		}

		case NodeKind::FOR_LOOP_STATEMENT:
		{
			Log::Print("{0}FOR_LOOP_STATEMENT:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.ForLoopStatement.InitExpressionID, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.ForLoopStatement.ConditionExpressionID, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.ForLoopStatement.IncrementExpressionID, indent + 1);
			printNode(tokenBuffers, nodeBuffers, currentNode.ForLoopStatement.BodyID, indent + 1);
			break;
		}

		case NodeKind::WHILE_LOOP_STATEMENT:
		{
			Log::Print("{0}WHILE_LOOP_STATEMENT:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.WhileLoopStatement.ConditionExpressionID, indent + 1);
			break;
		}

		case NodeKind::IF_STATEMENT:
		{
			Log::Print("{0}IF_STATEMENT:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.IfStatement.ConditionExpressionID, indent + 1);
			break;
		}

		case NodeKind::BLOCK_STATEMENT:
		{
			Log::Print("{0}BLOCK_STATEMENT:", indentStr);
			for (const auto& statementID : currentNode.BlockStatement.StatementIDs)
			{
				printNode(tokenBuffers, nodeBuffers, statementID, indent + 1);
			}
			break;
		}

		case NodeKind::RETURN_STATEMENT:
		{
			Log::Print("{0}RETURN_STATEMENT:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.ReturnStatement.ExpressionID, indent + 1);
			break;
		}

		case NodeKind::VALUE_DEFINITION:
		{
			Log::Print("{0}VALUE_DEFINITION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.ValueDefinition.ValueDeclarationID, indent + 1);
			break;
		}

		case NodeKind::STRUCT_DEFINITION:
		{
			Log::Print("{0}STRUCT_DEFINITION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.StructDefinition.IdentifierID, indent + 1);
			break;
		}

		case NodeKind::ENUM_DEFINITION:
		{
			Log::Print("{0}ENUM_DEFINITION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.EnumDefinition.IdentifierID, indent + 1);
			break;
		}

		case NodeKind::FUNCTION_DEFINITION:
		{
			Log::Print("{0}FUNCTION_DEFINITION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.FunctionDefinition.IdentifierID, indent + 1);
			break;
		}

		case NodeKind::QUALIFIED_DEFINITION:
		{
			Log::Print("{0}QUALIFIED_DEFINITION:", indentStr);
			printNode(tokenBuffers, nodeBuffers, currentNode.QualifiedDefinition.DefinitionID, indent + 1);
			break;
		}

		case NodeKind::MODULE:
		{
			Log::Print("{0}MODULE:", indentStr);
			for (const auto& definitionID : currentNode.Module.QualifiedDefinitionIDs)
			{
				printNode(tokenBuffers, nodeBuffers, definitionID, indent + 1);
			}
			break;
		}

		case NodeKind::PROGRAM:
		{
			Log::Print("{0}PROGRAM:", indentStr);
			for (const auto& moduleID : currentNode.Program.ModuleIDs)
			{
				printNode(tokenBuffers, nodeBuffers, moduleID, indent + 1);
			}
			break;
		}


		default:
			Log::Print("{0}Unknown node kind: {1}", indentStr, (size_t)currentNode.Kind);
			break;

		}
	}

	void PrintTree(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers)
	{
		printNode(tokenBuffers, nodeBuffers, nodeBuffers.GetRootNodeID(), 0);
	}
}


