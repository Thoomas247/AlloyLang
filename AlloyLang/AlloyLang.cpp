#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"

#include "log/Log.hpp"

#include <chrono>

constexpr auto TestSrc = R"(
fn do_nothing (var a : i64, const b : &bool)
{}

pub fn make_tuple (const a : &i64, const b : &bool) -> (const i64, var bool)
{
	return (a, b);
}

pub var s : String = "ABC";
exp var a : i64 = 0;
var c : char = 'A';
var d : char = '\0';
var e : f32 = 1.01;

const b : i8 = 0;

)";

int main()
{
	auto start = std::chrono::high_resolution_clock::now();

	auto buffers = AlloyCompiler::Tokenizer::Tokenize(TestSrc);
	auto application = AlloyCompiler::Parser::Parse(buffers);

	auto end = std::chrono::high_resolution_clock::now();

	AlloyCompiler::Log::Print("\n-- Compiled in {0}ms --", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}