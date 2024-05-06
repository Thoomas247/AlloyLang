#pragma once
#include "../log/Log.hpp"
#include "../tokenizer/Token.hpp"
#include "NodeEnums.hpp"

namespace AlloyCompiler
{
	enum class NodeKind : uint8_t 
	{
		None = -1
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

	private:
		inline static NodeKind s_Counter = (NodeKind)0;

		template <typename T>
		static NodeKind getKind()
		{
			using UnderlyingType = std::underlying_type_t<NodeKind>;

			static NodeKind kind = static_cast<NodeKind>(++static_cast<UnderlyingType>(s_Counter));

			return kind;
		}
	};

	/// <summary>
	/// Type-safe alternative to unions to store nodes.
	/// Checks that the type being accessed is the type stored in the union.
	/// </summary>
	template <typename... Ts>
	class VariantNode
	{
	public:
		VariantNode()
			: m_CurrentKind(NodeKind::None), m_Data({})
		{

		}

		template <typename T>
		VariantNode(T&& value)
			: m_CurrentKind(NodeKind::None), m_Data({})
		{
			Set<T>(value);
		}

		template <typename T>
		void Set(T&& value)
		{
			ASSERT((std::is_same_v<T, Ts> || ...), "This variant cannot hold the given type!");

			getDataAs<T>() = value;
			m_CurrentKind = NodeInfo::Kind<T>();
		}

		template <typename T>
		T& Get()
		{
			ASSERT(m_CurrentKind == NodeInfo::Kind<T>(), "Variant holds a different type to the given type!");

			return getDataAs<T>();
		}

	private:
		template <typename T>
		T& getDataAs()
		{
			return static_cast<T>(m_Data);
		}

	private:
		static constexpr size_t MAX_SIZE = std::max({ sizeof(Ts)... });

	private:
		NodeKind m_CurrentKind;	// the current kind of the variant node
		char m_Data[MAX_SIZE];	// to match the size of our largest type
	};

	using NODE = VariantNode<>;

	using QUERYABLE = VariantNode<>;
	using DEFINITION = VariantNode<>;
	using EXPRESSION = VariantNode<>;
	using PRIMARY = VariantNode<>;
	using ASSIGNABLE = VariantNode<>;
	using POSTFIX = VariantNode<>;
	using STATEMENT = VariantNode<>;
}