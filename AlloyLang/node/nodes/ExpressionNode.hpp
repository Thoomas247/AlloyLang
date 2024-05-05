#pragma once
#include "../Node.hpp"

#include "IdentifierNode.hpp"
#include "DeclarationNode.hpp"
#include "../../tokenizer/Token.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// EXPRESSION:		BINARY_EXPRESSION 
	///					| UNARY_EXPRESSION 
	///					| PRIMARY_EXPRESSION 
	///					| ASSIGNMENT_EXPRESSION 
	///					;
	/// </summary>
	class ExpressionNode : public Node
	{
	public:
		ExpressionNode() = default;
		virtual ~ExpressionNode() = 0;
	};

	/// <summary>
	/// PRIMARY_EXPRESSION:		IDENTIFIER 
	///							| LITERAL 
	///							| CONSTRUCTOR_EXPRESSION 
	///							| POINTER_INITIALIZER_EXPRESSION 
	///							| POINTER_MOVE_EXPRESSION 
	///							| INITIALIZER_LIST_EXPRESSION 
	///							| VALUE_DEFINITION_EXPRESSION 
	///							| ENCLOSED_EXPRESSION 
	///							| FUNCTION_CALL_EXPRESSION 
	///							;
	/// </summary>
	class PrimaryExpressionNode : public ExpressionNode
	{
	public:
		PrimaryExpressionNode() = default;
		virtual ~PrimaryExpressionNode() = 0;
	};

	/// <summary>
	/// POSTFIX_EXPRESSION:		ARRAY_ACCESS_EXPRESSION 
	///							| MEMBER_ACCESS_EXPRESSION 
	///							;
	/// </summary>
	class PostfixExpressionNode : public Node
	{
	public:
		PostfixExpressionNode() = default;
		virtual ~PostfixExpressionNode() = 0;
	};

	/// <summary>
	/// CONSTRUCTOR_EXPRESSION:	IDENTIFIER open_brace ( IDENTIFIER assignment_operator EXPRESSION ) { comma IDENTIFIER assignment_operator EXPRESSION } close_brace ;
	/// </summary>
	class ConstructorExpressionNode : public Node
	{
	public:
		ConstructorExpressionNode(IdentifierNode* pTypeName, const std::vector<IdentifierNode*>& memberNames, const std::vector<IdentifierNode*>& memberValues)
			: pTypeName(pTypeName), MemberNames(memberNames), MemberValues(memberValues)
		{
		}

	public:
		IdentifierNode* pTypeName;
		std::vector<IdentifierNode*> MemberNames;	// name of the member to assign the corresponding value in MemberValues to
		std::vector<IdentifierNode*> MemberValues;	// value to assign to the corresponding member in MemberNames
	};

	/// <summary>
	/// POINTER_INITIALIZER_EXPRESSION:	new_keyword ( EXPRESSION | ( open_bracket EXPRESSION semicolon EXPRESSION close_bracket ) ) ;
	/// </summary>
	class PointerInitializerExpressionNode : public Node
	{
	public:
		PointerInitializerExpressionNode(ExpressionNode* pValue, ExpressionNode* pCount)
			: pValue(pValue), pCount(pCount)
		{
		}

	public:
		ExpressionNode* pValue;
		ExpressionNode* pCount;	// can be nullptr if not an array
	};

	/// <summary>
	/// POINTER_MOVE_EXPRESSION:	move_keyword IDENTIFIER ;
	/// </summary>
	class PointerMoveExpressionNode : public Node
	{
	public:
		PointerMoveExpressionNode(IdentifierNode* pPointerName)
			: pPointerName(pPointerName)
		{
		}

	public:
		IdentifierNode* pPointerName;
	};

	/// <summary>
	/// INITIALIZER_LIST_EXPRESSION:	open_brace [ EXPRESSION { comma EXPRESSION } ] close_brace ;
	/// </summary>
	class InitializerListExpressionNode : public Node
	{
	public:
		InitializerListExpressionNode(const std::vector<ExpressionNode*>& values)
			: Values(values)
		{
		}

	public:
		std::vector<ExpressionNode*> Values;
	};

	/// <summary>
	/// VALUE_DEFINITION_EXPRESSION:	VALUE_DECLARATION assignment_operator EXPRESSION ;
	/// </summary>
	class ValueDefinitionExpressionNode : public Node
	{
	public:
		ValueDefinitionExpressionNode(ValueDeclarationNode* pValueDeclaration, ExpressionNode* pValue)
			: pValueDeclaration(pValueDeclaration), pValue(pValue)
		{
		}

	public:
		ValueDeclarationNode* pValueDeclaration;
		ExpressionNode* pValue;
	};

	/// <summary>
	/// ENCLOSED_EXPRESSION:	open_paren EXPRESSION close_paren ;
	/// </summary>
	class EnclosedExpressionNode : public Node
	{
	public:
		EnclosedExpressionNode(ExpressionNode* pExpression)
			: pExpression(pExpression)
		{
		}

	public:
		ExpressionNode* pExpression;
	};

	/// <summary>
	/// FUNCTION_CALL_EXPRESSION:	IDENTIFIER open_paren [ EXPRESSION { comma EXPRESSION } ] close_paren ;
	/// </summary>
	class FunctionCallExpressionNode : public Node
	{
	public:
		FunctionCallExpressionNode(IdentifierNode* pFunctionName, const std::vector<ExpressionNode*>& arguments)
			: pFunctionName(pFunctionName), Arguments(arguments)
		{
		}

	public:
		IdentifierNode* pFunctionName;
		std::vector<ExpressionNode*> Arguments;
	};

	/// <summary>
	/// ARRAY_ACCESS_EXPRESSION:	PRIMARY_EXPRESSION open_bracket EXPRESSION close_bracket ;
	/// </summary>
	class ArrayAccessExpressionNode : public Node
	{
	public:
		ArrayAccessExpressionNode(PrimaryExpressionNode* pArray, ExpressionNode* pIndex)
			: pArray(pArray), pIndex(pIndex)
		{
		}

	public:
		PrimaryExpressionNode* pArray;
		ExpressionNode* pIndex;
	};

	/// <summary>
	/// MEMBER_ACCESS_EXPRESSION:	PRIMARY_EXPRESSION dot IDENTIFIER ;
	/// </summary>
	class MemberAccessExpressionNode : public Node
	{
	public:
		MemberAccessExpressionNode(PrimaryExpressionNode* pStruct, IdentifierNode* pMember)
			: pStruct(pStruct), pMember(pMember)
		{
		}

	public:
		PrimaryExpressionNode* pStruct;
		IdentifierNode* pMember;
	};

	/// <summary>
	/// BINARY_EXPRESSION:	EXPRESSION binary_operator EXPRESSION ;
	/// </summary>
	class BinaryExpressionNode : public Node
	{
	public:
		BinaryExpressionNode(TokenID operatorTokenID, ExpressionNode* pLeft, ExpressionNode* pRight)
			: OperatorTokenID(operatorTokenID), pLeft(pLeft), pRight(pRight)
		{
		}

	public:
		TokenID OperatorTokenID;
		ExpressionNode* pLeft;
		ExpressionNode* pRight;
	};

	/// <summary>
	/// UNARY_EXPRESSION:	unary_operator POSTFIX_EXPRESSION ;
	/// </summary>
	class UnaryExpressionNode : public Node
	{
	public:
		UnaryExpressionNode(TokenID operatorTokenID, PostfixExpressionNode* pExpression)
			: OperatorTokenID(operatorTokenID), pExpression(pExpression)
		{
		}

	public:
		TokenID OperatorTokenID;
		PostfixExpressionNode* pExpression;
	};

	/// <summary>
	/// ASSIGNMENT_EXPRESSION:	( POSTFIX_EXPRESSION | IDENTIFIER ) assignment_operator EXPRESSION ;
	/// </summary>
	class AssignmentExpressionNode : public Node
	{
	public:

	};


}