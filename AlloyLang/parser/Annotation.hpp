#pragma once
#include <unordered_map>
#include <string_view>

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
		{ "exclude", AnnotationKind::MultiArgs }
	};

	using AnnotationArgs = std::vector<std::string_view>;

	using AnnotationMap = std::unordered_map<std::string_view, AnnotationArgs>;
}