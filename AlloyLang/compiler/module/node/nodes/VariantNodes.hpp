#pragma once
#include "../../../../log/Log.hpp"

namespace AlloyCompiler
{
	enum class NodeKind : uint8_t
	{
		None = std::numeric_limits<uint8_t>::max()
	};

	/// <summary>
	/// Helper class to create a unique ID per Node type.
	/// </summary>
	class NodeInfo
	{
	public:
		template <typename T>
		static NodeKind Kind()
		{
			return getKind<std::remove_const_t<std::remove_reference_t<std::remove_const_t<T>>>>();
		}

		template <typename T>
		static std::string Name()
		{
			return typeid(T).name();
		}

	private:
		using UnderlyingType = std::underlying_type_t<NodeKind>;
		inline static UnderlyingType s_Counter = 0;

		template <typename T>
		static NodeKind getKind()
		{
			static NodeKind kind = static_cast<NodeKind>(++s_Counter);

			ASSERT(kind != NodeKind::None, "NodeKind overflow!");

			return kind;
		}
	};

	/// <summary>
	/// Type-safe pointer to a node of any type.
	/// </summary>
	template<typename ... Ts>
	struct VariantNode
	{
	public:
		VariantNode()
			: m_Kind(NodeKind::None), m_pNode(nullptr)
		{}

		template <typename T>
		VariantNode(T* pNode)
		{
			Set(pNode);
		}

		template <typename T>
		void Set(T* pNode)
		{
			ASSERT((std::is_same_v<T, Ts> || ...), "Variant node cannot hold the given type!");

			m_Kind = NodeInfo::Kind<T>();
			m_pNode = pNode;

#ifdef _DEBUG
			m_Name = NodeInfo::Name<T>();
#endif // _DEBUG
		}

		template <typename T>
		T* Get() const
		{
			ASSERT(m_Kind == NodeInfo::Kind<T>(), "Variant holds a different type to the given type!");

			return (T*)m_pNode;
		}

		template <typename T>
		bool Is() const
		{
			return m_Kind == NodeInfo::Kind<T>();
		}

		NodeKind GetKind() const
		{
			return m_Kind;
		}

		bool IsEmpty() const
		{
			return m_pNode == nullptr;
		}

	private:
		NodeKind m_Kind;	// the kind of the variant node
		void* m_pNode;		// points to the variant node

#ifdef _DEBUG
		std::string m_Name;
#endif // _DEBUG

	};


	struct LITERAL;
	struct VARIABLE;
	struct VARIABLE_DEFINITION;
	struct FUNCTION_CALL;
	struct CONSTRUCTOR;
	struct POINTER_INIT;
	struct POINTER_MOVE;
	struct INITIALIZER_LIST;
	struct ENCLOSED_EXPRESSION;
	struct ENUM_VALUE;
	struct ASSIGNMENT;
	struct FOR_LOOP;
	struct WHILE_LOOP;
	struct IF_STATEMENT;
	struct SWITCH_STATEMENT;
	struct STATEMENT_BLOCK;
	struct RETURN;
	struct ARRAY_ACCESS;
	struct MEMBER_ACCESS;
	struct UNARY;
	struct BINARY;
	struct VARIABLE_DECLARATION;
	struct GENERIC_PARAMETER;
	struct MACRO_DEFINITION;
	struct TYPE_DEFINITION;
	struct FUNCTION_DEFINITION;
	struct EXTERN_DEFINITION;
	
	using PRIMARY = VariantNode<LITERAL, VARIABLE, VARIABLE_DEFINITION, FUNCTION_CALL, CONSTRUCTOR,
		POINTER_INIT, POINTER_MOVE, INITIALIZER_LIST, ENCLOSED_EXPRESSION, ENUM_VALUE>;

	using STATEMENT = VariantNode<VARIABLE_DEFINITION, FUNCTION_CALL, ASSIGNMENT, FOR_LOOP, WHILE_LOOP,
		IF_STATEMENT, SWITCH_STATEMENT, STATEMENT_BLOCK, RETURN>;

	using POSTFIX = VariantNode<ARRAY_ACCESS, MEMBER_ACCESS>;

	using EXPRESSION = VariantNode<PRIMARY, POSTFIX, UNARY, BINARY, ASSIGNMENT>;

	using FUNCTION_PARAMETER = VariantNode<GENERIC_PARAMETER, VARIABLE_DECLARATION>;
}