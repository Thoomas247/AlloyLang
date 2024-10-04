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
		static const char* Name()
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
			, m_NextVariantKind(NodeKind::None), m_pNextVariantNode(nullptr)
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

			m_NextVariantKind = NodeKind::None;
			m_pNextVariantNode = nullptr;

#ifdef _DEBUG
			m_pName = NodeInfo::Name<T>();
#endif // _DEBUG
		}

		template <typename... Ns>
		void Set(VariantNode<Ns...>* pVariantNode)
		{
			ASSERT((std::is_same_v<VariantNode<Ns...>, Ts> || ...), "Variant node cannot hold the given type!");

			m_Kind = pVariantNode->_getKind();
			m_pNode = pVariantNode->_getNode();

			m_NextVariantKind = NodeInfo::Kind<VariantNode<Ns...>>();
			m_pNextVariantNode = pVariantNode;

#ifdef _DEBUG
			m_pName = pVariantNode->GetName();
#endif // _DEBUG
		}

		template <typename T>
		T* Get() const
		{
			if (NodeInfo::Kind<T>() == m_Kind)
			{
				return (T*)m_pNode;
			}

			else if (NodeInfo::Kind<T>() == m_NextVariantKind)
			{
				return (T*)m_pNextVariantNode;
			}

			ASSERT(false, "Variant holds a different type to the given type!");

			return nullptr;
		}

		template <typename T>
		bool Is() const
		{
			return NodeInfo::Kind<T>() == m_Kind || NodeInfo::Kind<T>() == m_NextVariantKind;
		}

		NodeKind GetKind() const
		{
			return m_Kind;
		}

		bool IsEmpty() const
		{
			return m_pNode == nullptr;
		}

#ifdef _DEBUG
		const char* GetName() const
		{
			return m_pName;
		}
#endif

		NodeKind _getKind() const
		{
			return m_Kind;
		}

		void* _getNode() const
		{
			return m_pNode;
		}

		NodeKind _getNextVariantKind() const
		{
			return m_NextVariantKind;
		}

		void* _getNextVariantNode() const
		{
			return m_pNextVariantNode;
		}

	private:
		NodeKind m_Kind;
		void* m_pNode;

		NodeKind m_NextVariantKind;
		void* m_pNextVariantNode;

#ifdef _DEBUG
		const char* m_pName;
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
	
	using PRIMARY = VariantNode<LITERAL, VARIABLE, VARIABLE_DEFINITION, FUNCTION_CALL, CONSTRUCTOR,
		POINTER_INIT, POINTER_MOVE, INITIALIZER_LIST, ENCLOSED_EXPRESSION, ENUM_VALUE>;

	using STATEMENT = VariantNode<VARIABLE_DEFINITION, FUNCTION_CALL, ASSIGNMENT, FOR_LOOP, WHILE_LOOP,
		IF_STATEMENT, SWITCH_STATEMENT, STATEMENT_BLOCK, RETURN>;

	using POSTFIX = VariantNode<ARRAY_ACCESS, MEMBER_ACCESS, FUNCTION_CALL>;

	using EXPRESSION = VariantNode<PRIMARY, POSTFIX, UNARY, BINARY, ASSIGNMENT>;

	using FUNCTION_PARAMETER = VariantNode<GENERIC_PARAMETER, VARIABLE_DECLARATION>;
}