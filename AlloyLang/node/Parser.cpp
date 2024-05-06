#include "Parser.hpp"

namespace AlloyCompiler
{
	struct ParsingState
	{
		constexpr static auto NUM_NODES_FACTOR = 1.5;

		NamedNodes NamedNodes;
		NodeAllocator Allocator;
		TokenBuffers::Iterator TokenIter;

		ParsingState(const TokenBuffers& tokenBuffers)
			: NamedNodes()
			, Allocator((size_t)((size_t)tokenBuffers.LastTokenID() * NUM_NODES_FACTOR))
			, TokenIter(tokenBuffers.GetIterator())
		{
		}
	};

	template <typename T>
	T* parse(ParsingState&) = delete;

	NamedNodes Parse(const TokenBuffers& tokenBuffers)
	{
		ParsingState state(tokenBuffers);

		//NAMED_GROUP_DEFINITION
		//NAMED_SYSTEM_DEFINITION
		//NAMED_QUERY_DEFINITION
		//NAMED_TYPE_DEFINITION
		//NAMED_FUNCTION_DEFINITION
		//EXTERN_FUNCTION_DEFINITION
		
		while (state.TokenIter.HasNext())
		{
			switch (state.TokenIter.GetKind())
			{
			case TokenKind::group_keyword:
				parse<NAMED_GROUP_DEFINITION>(state);

			case TokenKind::system_keyword:
				parse<NAMED_SYSTEM_DEFINITION>(state);

			case TokenKind::query_keyword:
				parse<NAMED_QUERY_DEFINITION>(state);

			case TokenKind::type_keyword:
				parse<NAMED_TYPE_DEFINITION>(state);

			case TokenKind::function_keyword:
				parse<NAMED_FUNCTION_DEFINITION>(state);

			case TokenKind::extern_keyword:
				parse<EXTERN_FUNCTION_DEFINITION>(state);

			default:
				break;
			}
		}

		return state.NamedNodes;
	}
}