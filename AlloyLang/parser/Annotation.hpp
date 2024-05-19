#pragma once
#include <unordered_map>
#include <string_view>

#include "Node.hpp"

namespace AlloyCompiler
{

	enum class AnnotationKind
	{
		NoArgs,
		SingleArg,
		MultiArgs
	};
	
	const std::unordered_map<std::string_view, AnnotationKind> ANNOTATION_NAMES =
	{
		// component annotations
		{ "exclude", AnnotationKind::MultiArgs },

		// system annotations
		{ "inline", AnnotationKind::NoArgs },
	};

	using AnnotationArgs = std::vector<Token*>;
	using AnnotationMap = std::unordered_map<std::string_view, AnnotationArgs>;
}