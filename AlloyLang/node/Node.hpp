#pragma once
#include "../log/Log.hpp"
#include "../util/SmallStringView.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// The base class of all node types.
	/// </summary>
	class Node
	{
	public:
		Node() = default;
		virtual ~Node() = 0;
	};

}