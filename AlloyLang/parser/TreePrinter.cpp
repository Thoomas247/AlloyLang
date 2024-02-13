#include "TreePrinter.hpp"

#include "../log/Log.hpp"
#include "../json/json.hpp"

using json = nlohmann::ordered_json;

namespace AlloyCompiler
{

	template <typename T>
	json print(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID) = delete;

	template <>
	json print<EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID);

	template <>
	json print<LITERAL>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& literalNode = nodeBuffers.GetNode(nodeID).Literal;

		std::string literalType = "NONE";

		if (literalNode.Kind == LITERAL::Type::Integer)
		{
			literalType = "INTEGER";
		}
		else if (literalNode.Kind == LITERAL::Type::Float)
		{
			literalType = "FLOAT";
		}
		else if (literalNode.Kind == LITERAL::Type::String)
		{
			literalType = "STRING";
		}
		else if (literalNode.Kind == LITERAL::Type::Boolean)
		{
			literalType = "BOOLEAN";
		}
		else if (literalNode.Kind == LITERAL::Type::Character)
		{
			literalType = "CHARACTER";
		}

		json j;
		j["kind"] = "LITERAL";
		j["type"] = literalType;
		j["value"] = tokenBuffers.GetValue(literalNode.InfoTokenID).ToStringView();

		return j;
	}

	template <>
	json print<POINTER_INITIALIZER_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& pointerInitializerExpressionNode = nodeBuffers.GetNode(nodeID).PointerInitializerExpression;

		json j;
		j["kind"] = "POINTER_INITIALIZER_EXPRESSION";
		j["value"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, pointerInitializerExpressionNode.ValueID);

		if (pointerInitializerExpressionNode.CountID != ERROR_NODE_ID)
		{
			j["array_size"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, pointerInitializerExpressionNode.CountID);
		}

		return j;
	}

	template <>
	json print<INITIALIZER_LIST_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& initializerListExpressionNode = nodeBuffers.GetNode(nodeID).InitializerListExpression;

		json j;
		j["kind"] = "INITIALIZER_LIST_EXPRESSION";

		for (const auto& expressionID : initializerListExpressionNode.ValueIDs)
		{
			j["values"].push_back(print<EXPRESSION>(tokenBuffers, nodeBuffers, expressionID));
		}

		return j;
	}

	template <>
	json print<IDENTIFIER>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& identifierNode = nodeBuffers.GetNode(nodeID).Identifier;

		json j;
		j["kind"] = "IDENTIFIER";
		j["value"] = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

		return j;
	}

	template <>
	json print<ARRAY_ACCESS_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& arrayAccessExpressionNode = nodeBuffers.GetNode(nodeID).ArrayAccessExpression;

		json j;
		j["kind"] = "ARRAY_ACCESS_EXPRESSION";
		j["array"] = print<IDENTIFIER>(tokenBuffers, nodeBuffers, arrayAccessExpressionNode.ArrayIdentifierID);
		j["index"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, arrayAccessExpressionNode.IndexExpressionID);

		return j;
	}

	template <>
	json print<TYPE_IDENTIFIER>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& typeIdentifierNode = nodeBuffers.GetNode(nodeID).TypeIdentifier;

		std::string modifierString = "NONE";

		if (typeIdentifierNode.Mod == TYPE_IDENTIFIER::Modifier::Reference)
		{
			modifierString = "REFERENCE";
		}

		else if (typeIdentifierNode.Mod == TYPE_IDENTIFIER::Modifier::Pointer)
		{
			modifierString = "POINTER";
		}

		json j;
		j["kind"] = "TYPE_IDENTIFIER";
		j["modifier"] = modifierString;

		if (nodeBuffers.GetNode(typeIdentifierNode.TypeIdentifierID).Kind == NodeKind::IDENTIFIER)
		{
			j["identifier"] = print<IDENTIFIER>(tokenBuffers, nodeBuffers, typeIdentifierNode.TypeIdentifierID);
		}
		else
		{
			j["type_identifier"] = print<TYPE_IDENTIFIER>(tokenBuffers, nodeBuffers, typeIdentifierNode.TypeIdentifierID);
		}

		if (typeIdentifierNode.ArraySizeID != ERROR_NODE_ID)
		{
			j["array_size"] = print<LITERAL>(tokenBuffers, nodeBuffers, typeIdentifierNode.ArraySizeID);
		}

		return j;
	}

	template <>
	json print<TYPE_DECLARATION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& typeDeclarationNode = nodeBuffers.GetNode(nodeID).TypeDeclaration;

		std::string typeString = "COPY";

		if (typeDeclarationNode.Kind == TYPE_DECLARATION::Type::Constant)
		{
			typeString = "CONSTANT";
		}
		else if (typeDeclarationNode.Kind == TYPE_DECLARATION::Type::Variable)
		{
			typeString = "VARIABLE";
		}

		json j;
		j["kind"] = "TYPE_DECLARATION";
		j["type"] = typeString;
		j["identifier"] = print<TYPE_IDENTIFIER>(tokenBuffers, nodeBuffers, typeDeclarationNode.TypeIdentifierID);

		return j;
	}

	template <>
	json print<VALUE_DECLARATION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& valueDeclarationNode = nodeBuffers.GetNode(nodeID).ValueDeclaration;

		json j;
		j["kind"] = "VALUE_DECLARATION";
		j["type"] = valueDeclarationNode.Kind == VALUE_DECLARATION::Type::Variable ? "VARIABLE" : "CONSTANT";
		j["identifier"] = print<IDENTIFIER>(tokenBuffers, nodeBuffers, valueDeclarationNode.IdentifierID);
		j["type"] = print<TYPE_IDENTIFIER>(tokenBuffers, nodeBuffers, valueDeclarationNode.TypeIdentifierID);

		return j;
	}

	template <>
	json print<FUNCTION_DECLARATION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& functionDeclarationNode = nodeBuffers.GetNode(nodeID).FunctionDeclaration;

		json j;
		j["kind"] = "FUNCTION_DECLARATION";
		j["identifier"] = print<IDENTIFIER>(tokenBuffers, nodeBuffers, functionDeclarationNode.IdentifierID);

		for (const auto& parameterID : functionDeclarationNode.ParameterIDs)
		{
			j["parameters"].push_back(print<VALUE_DECLARATION>(tokenBuffers, nodeBuffers, parameterID));
		}

		if (functionDeclarationNode.ReturnTypeID != ERROR_NODE_ID)
		{
			j["return_type"] = print<TYPE_DECLARATION>(tokenBuffers, nodeBuffers, functionDeclarationNode.ReturnTypeID);
		}

		return j;
	}

	template <>
	json print<VALUE_DEFINITION_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& valueDefinitionExpressionNode = nodeBuffers.GetNode(nodeID).ValueDefinitionExpression;

		json j;
		j["kind"] = "VALUE_DEFINITION_EXPRESSION";
		j["declaration"] = print<VALUE_DECLARATION>(tokenBuffers, nodeBuffers, valueDefinitionExpressionNode.ValueDeclarationID);
		j["value"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, valueDefinitionExpressionNode.ValueID);

		return j;
	}

	template <>
	json print<FUNCTION_CALL_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& functionCallExpressionNode = nodeBuffers.GetNode(nodeID).FunctionCallExpression;

		json j;
		j["kind"] = "FUNCTION_CALL_EXPRESSION";
		j["function_name"] = print<IDENTIFIER>(tokenBuffers, nodeBuffers, functionCallExpressionNode.IdentifierID);

		for (const auto& argumentID : functionCallExpressionNode.ArgumentIDs)
		{
			j["arguments"].push_back(print<EXPRESSION>(tokenBuffers, nodeBuffers, argumentID));
		}

		return j;
	}

	template <>
	json print<ENCLOSED_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& enclosedExpressionNode = nodeBuffers.GetNode(nodeID).EnclosedExpression;

		json j;
		j["kind"] = "ENCLOSED_EXPRESSION";
		j["expression"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, enclosedExpressionNode.ExpressionID);

		return j;
	}

	template <>
	json print<BINARY_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& binaryExpressionNode = nodeBuffers.GetNode(nodeID).BinaryExpression;

		json j;
		j["kind"] = "BINARY_EXPRESSION";
		j["operator"] = tokenBuffers.GetValue(binaryExpressionNode.OperatorTokenID).ToStringView();
		j["left"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, binaryExpressionNode.LeftID);
		j["right"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, binaryExpressionNode.RightID);

		return j;
	}

	template <>
	json print<UNARY_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& unaryExpressionNode = nodeBuffers.GetNode(nodeID).UnaryExpression;

		json j;
		j["kind"] = "UNARY_EXPRESSION";
		j["operator"] = tokenBuffers.GetValue(unaryExpressionNode.OperatorTokenID).ToStringView();
		j["operand"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, unaryExpressionNode.OperandID);

		return j;
	}

	template <>
	json print<PRIMARY_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& primaryExpressionNode = nodeBuffers.GetNode(nodeID);

		switch (primaryExpressionNode.Kind)
		{
		case NodeKind::IDENTIFIER:
			return print<IDENTIFIER>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::LITERAL:
			return print<LITERAL>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::POINTER_INITIALIZER_EXPRESSION:
			return print<POINTER_INITIALIZER_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::INITIALIZER_LIST_EXPRESSION:
			return print<INITIALIZER_LIST_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::VALUE_DEFINITION_EXPRESSION:
			return print<VALUE_DEFINITION_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::ARRAY_ACCESS_EXPRESSION:
			return print<ARRAY_ACCESS_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::FUNCTION_CALL_EXPRESSION:
			return print<FUNCTION_CALL_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::ENCLOSED_EXPRESSION:
			return print<ENCLOSED_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		default:
			ASSERT(false, "Unknown primary expression kind");
			return json();
		}
	}

	template <>
	json print<ASSIGNMENT_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& assignmentExpressionNode = nodeBuffers.GetNode(nodeID).AssignmentExpression;

		json j;
		j["kind"] = "ASSIGNMENT_EXPRESSION";
		j["identifier"] = print<IDENTIFIER>(tokenBuffers, nodeBuffers, assignmentExpressionNode.IdentifierID);
		j["value"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, assignmentExpressionNode.ValueID);

		return j;
	}

	template <>
	json print<EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& expressionNode = nodeBuffers.GetNode(nodeID);

		switch (expressionNode.Kind)
		{
		case NodeKind::BINARY_EXPRESSION:
			return print<BINARY_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::UNARY_EXPRESSION:
			return print<UNARY_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::IDENTIFIER:
		case NodeKind::LITERAL:
		case NodeKind::POINTER_INITIALIZER_EXPRESSION:
		case NodeKind::INITIALIZER_LIST_EXPRESSION:
		case NodeKind::VALUE_DEFINITION_EXPRESSION:
		case NodeKind::ARRAY_ACCESS_EXPRESSION:
		case NodeKind::FUNCTION_CALL_EXPRESSION:
		case NodeKind::ENCLOSED_EXPRESSION:
			return print<PRIMARY_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::ASSIGNMENT_EXPRESSION:
			return print<ASSIGNMENT_EXPRESSION>(tokenBuffers, nodeBuffers, nodeID);

		default:
			ASSERT(false, "Unknown expression kind");
			return json();
		}
	}

	template <>
	json print<STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID);

	template <>
	json print<VALUE_DEFINITION_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& valueDefinitionStatementNode = nodeBuffers.GetNode(nodeID).ValueDefinitionStatement;

		json j;
		j["kind"] = "VALUE_DEFINITION_STATEMENT";
		j["definition_expression"] = print<VALUE_DEFINITION_EXPRESSION>(tokenBuffers, nodeBuffers, valueDefinitionStatementNode.ValueDefinitionExpressionID);

		return j;
	}

	template <>
	json print<FUNCTION_CALL_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& functionCallStatementNode = nodeBuffers.GetNode(nodeID).FunctionCallStatement;

		json j;
		j["kind"] = "FUNCTION_CALL_STATEMENT";
		j["function_call_expression"] = print<FUNCTION_CALL_EXPRESSION>(tokenBuffers, nodeBuffers, functionCallStatementNode.FunctionCallExpressionID);

		return j;
	}

	template <>
	json print<ASSIGNMENT_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& assignmentNode = nodeBuffers.GetNode(nodeID).AssignmentStatement;

		json j;
		j["kind"] = "ASSIGNMENT_STATEMENT";
		j["assignment_expression"] = print<ASSIGNMENT_EXPRESSION>(tokenBuffers, nodeBuffers, assignmentNode.AssignmentExpressionID);

		return j;
	}

	template <>
	json print<FOR_LOOP_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& forLoopNode = nodeBuffers.GetNode(nodeID).ForLoopStatement;

		json j;
		j["kind"] = "FOR_LOOP_STATEMENT";
		j["initializer"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, forLoopNode.InitExpressionID);
		j["condition"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, forLoopNode.ConditionExpressionID);
		j["increment"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, forLoopNode.IncrementExpressionID);
		j["body"] = print<STATEMENT>(tokenBuffers, nodeBuffers, forLoopNode.BodyID);

		return j;
	}

	template <>
	json print<WHILE_LOOP_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& whileLoopNode = nodeBuffers.GetNode(nodeID).WhileLoopStatement;

		json j;
		j["kind"] = "WHILE_LOOP_STATEMENT";
		j["condition"] = print<ENCLOSED_EXPRESSION>(tokenBuffers, nodeBuffers, whileLoopNode.ConditionExpressionID);
		j["body"] = print<STATEMENT>(tokenBuffers, nodeBuffers, whileLoopNode.BodyID);

		return j;
	}

	template <>
	json print<IF_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& ifStatementNode = nodeBuffers.GetNode(nodeID).IfStatement;

		json j;
		j["kind"] = "IF_STATEMENT";
		j["condition"] = print<ENCLOSED_EXPRESSION>(tokenBuffers, nodeBuffers, ifStatementNode.ConditionExpressionID);
		j["body"] = print<STATEMENT>(tokenBuffers, nodeBuffers, ifStatementNode.BodyID);

		if (ifStatementNode.ElseID != ERROR_NODE_ID)
		{
			j["else"] = print<STATEMENT>(tokenBuffers, nodeBuffers, ifStatementNode.ElseID);
		}

		return j;
	}

	template <>
	json print<BLOCK_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& blockStatementNode = nodeBuffers.GetNode(nodeID).BlockStatement;

		json j;
		j["kind"] = "BLOCK_STATEMENT";

		for (const auto& statementID : blockStatementNode.StatementIDs)
		{
			j["statements"].push_back(print<STATEMENT>(tokenBuffers, nodeBuffers, statementID));
		}

		return j;
	}

	template <>
	json print<RETURN_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& returnStatementNode = nodeBuffers.GetNode(nodeID).ReturnStatement;

		json j;
		j["kind"] = "RETURN_STATEMENT";

		if (returnStatementNode.ExpressionID != ERROR_NODE_ID)
		{
			j["expression"] = print<EXPRESSION>(tokenBuffers, nodeBuffers, returnStatementNode.ExpressionID);
		}

		return j;
	}

	template <>
	json print<STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& statementNode = nodeBuffers.GetNode(nodeID);

		switch (statementNode.Kind)
		{
		case NodeKind::VALUE_DEFINITION_STATEMENT:
			return print<VALUE_DEFINITION_STATEMENT>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::FUNCTION_CALL_STATEMENT:
			return print<FUNCTION_CALL_STATEMENT>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::ASSIGNMENT_STATEMENT:
			return print<ASSIGNMENT_STATEMENT>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::FOR_LOOP_STATEMENT:
			return print<FOR_LOOP_STATEMENT>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::WHILE_LOOP_STATEMENT:
			return print<WHILE_LOOP_STATEMENT>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::IF_STATEMENT:
			return print<IF_STATEMENT>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::BLOCK_STATEMENT:
			return print<BLOCK_STATEMENT>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::RETURN_STATEMENT:
			return print<RETURN_STATEMENT>(tokenBuffers, nodeBuffers, nodeID);

		default:
			ASSERT(false, "Unknown statement kind");
			return json();
		}
	}

	template <>
	json print<EXTERN_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& externDefinitionNode = nodeBuffers.GetNode(nodeID).ExternDefinition;

		json j;
		j["kind"] = "EXTERN_DEFINITION";
		j["declaration"] = print<FUNCTION_DECLARATION>(tokenBuffers, nodeBuffers, externDefinitionNode.FunctionDeclarationID);

		return j;
	}

	template <>
	json print<VALUE_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& valueDefinitionNode = nodeBuffers.GetNode(nodeID).ValueDefinition;

		json j;
		j["kind"] = "VALUE_DEFINITION";
		j["definition_expression"] = print<VALUE_DEFINITION_EXPRESSION>(tokenBuffers, nodeBuffers, valueDefinitionNode.ValueDefinitionExpressionID);

		return j;
	}

	template <>
	json print<STRUCT_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& structDefinitionNode = nodeBuffers.GetNode(nodeID).StructDefinition;

		json j;
		j["kind"] = "STRUCT_DEFINITION";
		j["name"] = print<IDENTIFIER>(tokenBuffers, nodeBuffers, structDefinitionNode.IdentifierID);

		for (const auto& memberID : structDefinitionNode.MemberIDs)
		{
			j["members"].push_back(print<VALUE_DECLARATION>(tokenBuffers, nodeBuffers, memberID));
		}

		return j;
	}

	template <>
	json print<ENUM_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& enumDefinitionNode = nodeBuffers.GetNode(nodeID).EnumDefinition;

		json j;
		j["kind"] = "ENUM_DEFINITION";
		j["name"] = print<IDENTIFIER>(tokenBuffers, nodeBuffers, enumDefinitionNode.IdentifierID);

		for (const auto& memberID : enumDefinitionNode.MemberIDs)
		{
			j["members"].push_back(print<IDENTIFIER>(tokenBuffers, nodeBuffers, memberID));
		}

		return j;
	}

	template <>
	json print<FUNCTION_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& functionDefinitionNode = nodeBuffers.GetNode(nodeID).FunctionDefinition;

		json j;
		j["kind"] = "FUNCTION_DEFINITION";
		j["declaration"] = print<FUNCTION_DECLARATION>(tokenBuffers, nodeBuffers, functionDefinitionNode.FunctionDeclarationID);
		j["body"] = print<BLOCK_STATEMENT>(tokenBuffers, nodeBuffers, functionDefinitionNode.BodyID);

		return j;
	}

	template <>
	json print<DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& definitionNode = nodeBuffers.GetNode(nodeID);

		switch (definitionNode.Kind)
		{
		case NodeKind::EXTERN_DEFINITION:
			return print<EXTERN_DEFINITION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::VALUE_DEFINITION:
			return print<VALUE_DEFINITION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::STRUCT_DEFINITION:
			return print<STRUCT_DEFINITION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::ENUM_DEFINITION:
			return print<ENUM_DEFINITION>(tokenBuffers, nodeBuffers, nodeID);

		case NodeKind::FUNCTION_DEFINITION:
			return print<FUNCTION_DEFINITION>(tokenBuffers, nodeBuffers, nodeID);

		default:
			ASSERT(false, "Unknown definition kind");
			return json();
		}
	}

	template <>
	json print<QUALIFIED_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& qualifiedDefinitionNode = nodeBuffers.GetNode(nodeID).QualifiedDefinition;

		json j;
		j["kind"] = "QUALIFIED_DEFINITION";
		j["qualifier"] = (size_t)qualifiedDefinitionNode.Visibility;

		j["definition"] = print<DEFINITION>(tokenBuffers, nodeBuffers, qualifiedDefinitionNode.DefinitionID);

		return j;
	}

	template <>
	json print<MODULE>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& moduleNode = nodeBuffers.GetNode(nodeID).Module;

		json j;
		j["kind"] = "MODULE";

		for (const auto& qualifiedDefinitionID : moduleNode.QualifiedDefinitionIDs)
		{
			j["qualified_definitions"].push_back(print<QUALIFIED_DEFINITION>(tokenBuffers, nodeBuffers, qualifiedDefinitionID));
		}

		return j;
	}

	template <>
	json print<PROGRAM>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID nodeID)
	{
		const auto& programNode = nodeBuffers.GetNode(nodeID).Program;

		json j;
		j["kind"] = "PROGRAM";

		for (const auto& moduleID : programNode.ModuleIDs)
		{
			j["modules"].push_back(print<MODULE>(tokenBuffers, nodeBuffers, moduleID));
		}

		return j;
	}

	void PrintTree(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID rootNode)
	{
		json j = print<PROGRAM>(tokenBuffers, nodeBuffers, rootNode);

		Log::Print("{0}", j.dump(NUM_SPACES_PER_TAB));


	}
}


