#pragma once

constexpr auto TestSrc = R"(
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

pub fn make_tuple (const a : &i64, const b : &bool) -> (const i64, var bool)
{
	return (a, b);
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