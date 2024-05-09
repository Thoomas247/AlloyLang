#pragma once

#include "../Node.hpp"

#include "nodes/ECSNode.hpp"
#include "nodes/ExpressionNode.hpp"
#include "nodes/FunctionNode.hpp"
#include "nodes/StatementNode.hpp"
#include "nodes/TypeNode.hpp"
#include "nodes/VariableNode.hpp"

namespace AlloyCompiler
{
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

	private:
		inline static NodeKind s_Counter = (NodeKind)0;

		template <typename T>
		static NodeKind getKind()
		{
			using UnderlyingType = std::underlying_type_t<NodeKind>;

			static NodeKind kind = static_cast<NodeKind>(++static_cast<UnderlyingType>(s_Counter));

			ASSERT(kind != NodeKind::None, "NodeKind overflow!");

			return kind;
		}
	};

	/// <summary>
	/// Type-safe pointer to a node of any type.
	/// </summary>
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
			m_Kind = NodeInfo::Kind<T>();
			m_pNode = pNode;
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

	private:
		NodeKind m_Kind;	// the kind of the variant node
		void* m_pNode;		// points to the variant node
	};
}