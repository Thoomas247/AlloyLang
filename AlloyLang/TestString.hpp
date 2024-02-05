#pragma once

constexpr auto TestSrc2 = R"(
const res : i64 = 1+2;
var e : f32 = res + 1.01;

fn add (const a : &i64, const b : &i64) -> i64
{
	return (a + b);
}

fn mul (const a : &i64, const b : &i64) -> i64
{
	return add(a, b);
};

)";

constexpr auto TestSrc1 = R"(
struct Vector3
{
	var x : f32;
	var y : f32;
	var z : f32;
}

enum TokenTypes
{
	Identifier,
	Number,
	Operator,
	Keyword,
	Comment,
	Whitespace,
	Newline,
	Unknown
}

fn do_nothing (var a : i64, const b : &bool)
{}

fn add (const a : &i64, const b : &i64) -> i64
{
	return a + b;
}

fn sub (const a : &i64, const b : &i64) -> i64
{
	return a - b;
}

fn mul (const a : &i64, const b : &i64) -> i64
{
	return a * b;
}

fn div (const a : &i64, const b : &i64) -> i64
{
	return a / b;
}

fn factorial (const a : &i64) -> i64
{
	if (a <= 1)
	{
		return 1;
	}
	else
	{
		return a * factorial(a - 1);
	}
}

fn loop (const a : &i64) -> i64
{
	var res : i64 = 0;
	for (var i : i64 = 0; i < a; i = i + 1)
	{
		res = res + i;
	}
	return res;
}

fn while_loop (const a : &i64) -> i64
{
	var res : i64 = 0;
	var i : i64 = 0;
	while (i < a)
	{
		res = res + i;
		i = i + 1;
	}
	return res;
}

fn anon_block (const a : &i64) -> i64
{
	{
		return a;
	}
}

exp var a : i64 = 2;
pub var c : char = 'A';

const res : i64 = div(a, 200);

var s : String = "ABC";
const d : char = '\0';
var e : f32 = 1.01;

const num : i32 = a + e / 50;

const b : i8 = 0;

)";