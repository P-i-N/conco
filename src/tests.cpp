#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/parts/doctest.cpp>

#include "conco/conco.hpp"
#include "conco/extras/conco_stl_types.hpp"

#include <print>
#include <tuple>

#define CHECK_NEXT_TOKEN( _Value ) \
	do \
	{ \
		auto token = tokenizer.next(); \
		REQUIRE( token.has_value() ); \
		REQUIRE( *token == _Value ); \
	} while ( false )

#define CHECK_NEXT_EMPTY_TOKEN \
	do \
	{ \
		auto token = tokenizer.next(); \
		REQUIRE( !token.has_value() ); \
	} while ( false )

TEST_SUITE( "Tokenizer tests" )
{
	TEST_CASE( "Empty string" )
	{
		conco::tokenizer tokenizer{ "" };
		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Basic split test" )
	{
		conco::tokenizer tokenizer{ R"(    first_token"second token", 'third token'  ,  fourth_token "'5th'" '"6th')" };

		CHECK_NEXT_TOKEN( "first_token" );
		CHECK_NEXT_TOKEN( "second token" );
		CHECK_NEXT_TOKEN( "third token" );
		CHECK_NEXT_TOKEN( "fourth_token" );
		CHECK_NEXT_TOKEN( "'5th'" );
		CHECK_NEXT_TOKEN( "\"6th" );

		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Unclosed string" )
	{
		conco::tokenizer tokenizer{ R"(    "unclosed string )" };

		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Escaping" )
	{
		conco::tokenizer tokenizer{ R"("X\"")" };

		CHECK_NEXT_TOKEN( "X\\\"" );
		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Brackets test" )
	{
		conco::tokenizer tokenizer{ R"(token1 {token2} {token3,token3} {})" };

		CHECK_NEXT_TOKEN( "token1" );
		CHECK_NEXT_TOKEN( "token2" );
		CHECK_NEXT_TOKEN( "token3,token3" );
		CHECK_NEXT_TOKEN( "" );

		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Nested brackets test" )
	{
		conco::tokenizer tokenizer{ R"(token1 {token2 {token3}, token2_end}, token4)" };

		CHECK_NEXT_TOKEN( "token1" );
		CHECK_NEXT_TOKEN( "token2 {token3}, token2_end" );
		CHECK_NEXT_TOKEN( "token4" );

		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "One big nest" )
	{
		conco::tokenizer tokenizer{ R"({{nested {brackets {1 {2 {3 {4}}}}} test} inside})" };

		CHECK_NEXT_TOKEN( "{nested {brackets {1 {2 {3 {4}}}}} test} inside" );

		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Unclosed brackets" )
	{
		conco::tokenizer tokenizer{ R"(token1 {token2 {token3} token2_end token4)" };

		CHECK_NEXT_TOKEN( "token1" );

		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Equal sign" )
	{
		conco::tokenizer tokenizer( "a=b c =d;e" );

		CHECK_NEXT_TOKEN( "a" );
		CHECK_NEXT_TOKEN( "=" );
		CHECK_NEXT_TOKEN( "b" );
		CHECK_NEXT_TOKEN( "c" );
		CHECK_NEXT_TOKEN( "=" );
		CHECK_NEXT_TOKEN( "d" );
		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Mixed tokens" )
	{
		conco::tokenizer tokenizer{ R"(
			first_token
			"second token"
			"third token with {braces}"
			{fourth_token with 'quotes', {braces} and "quoted {braces}"}
			fifth_token
			)" };

		CHECK_NEXT_TOKEN( "first_token" );
		CHECK_NEXT_TOKEN( "second token" );
		CHECK_NEXT_TOKEN( "third token with {braces}" );
		CHECK_NEXT_TOKEN( "fourth_token with 'quotes', {braces} and \"quoted {braces}\"" );
		CHECK_NEXT_TOKEN( "fifth_token" );

		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Semicolon stop" )
	{
		std::string input = R"(token1 token2;tokenizer should not touch this part)";
		auto semicolon_pos = input.find( ';' );
		REQUIRE( semicolon_pos != std::string::npos );

		conco::tokenizer tokenizer{ input };

		CHECK_NEXT_TOKEN( "token1" );
		CHECK_NEXT_TOKEN( "token2" );
		CHECK_NEXT_EMPTY_TOKEN;

		// TODO
		CHECK( !tokenizer.text.empty() );
		CHECK( tokenizer.text.data() == input.data() + semicolon_pos );
	}

	TEST_CASE( "Semicolon inside block" )
	{
		conco::tokenizer tokenizer{ R"(token1 {token2; should not be visible} token3)" };

		CHECK_NEXT_TOKEN( "token1" );
		CHECK_NEXT_EMPTY_TOKEN; // Semicolon inside block produces unclosed block token
	}

	TEST_CASE( "Escaped semicolon inside block" )
	{
		conco::tokenizer tokenizer{ R"(token1 {token2\; should be visible} token3)" };

		CHECK_NEXT_TOKEN( "token1" );
		CHECK_NEXT_TOKEN( "token2\\; should be visible" );
		CHECK_NEXT_TOKEN( "token3" );
		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Semicolon inside quotes" )
	{
		conco::tokenizer tokenizer{ R"(token1 'token2; should be visible' token3)" };

		CHECK_NEXT_TOKEN( "token1" );
		CHECK_NEXT_TOKEN( "token2; should be visible" );
		CHECK_NEXT_TOKEN( "token3" );
		CHECK_NEXT_EMPTY_TOKEN;
	}

	TEST_CASE( "Escaping" )
	{
		conco::tokenizer tokenizer( "\\'token xxx \\\\'yyy' \\;semicolon" );

		CHECK_NEXT_TOKEN( "\\'token" );
		CHECK_NEXT_TOKEN( "xxx" );
		CHECK_NEXT_TOKEN( "\\\\" );
		CHECK_NEXT_TOKEN( "yyy" );
		CHECK_NEXT_TOKEN( "\\;semicolon" );
		CHECK_NEXT_EMPTY_TOKEN;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CHECK_FROM_STRING( _Type, _Str, _ExpectedValue, _HasValue ) \
	do \
	{ \
		auto opt = conco::from_string( conco::tag<_Type>{}, _Str ); \
		REQUIRE( opt.has_value() == _HasValue ); \
		if ( _HasValue ) \
			REQUIRE( *opt == _ExpectedValue ); \
	} while ( false )

#define CHECK_TO_CHARS( _Type, _Value, _ExpectedStr ) \
	do \
	{ \
		char buffer[64] = { 0 }; \
		REQUIRE( conco::to_chars( conco::tag<_Type>{}, buffer, _Value ) > 0 ); \
		REQUIRE( std::string_view( buffer ) == _ExpectedStr ); \
	} while ( false )

TEST_SUITE( "Type conversions" )
{
	TEST_CASE( "Basic types" )
	{
		CHECK_FROM_STRING( int, "123", 123, true );
		CHECK_FROM_STRING( int, "0x123", 0x123, true );
		CHECK_FROM_STRING( int, "0b11001010", 0b11001010, true );
		CHECK_FROM_STRING( float, "1.0", 1.0, true );
		CHECK_FROM_STRING( double, "2.0", 2.0, true );
		CHECK_FROM_STRING( int, "abc", 0, false );
		CHECK_FROM_STRING( bool, "true", true, true );
		CHECK_FROM_STRING( bool, "false", false, true );
		CHECK_FROM_STRING( bool, "1", true, true );
		CHECK_FROM_STRING( bool, "0", false, true );
		CHECK_FROM_STRING( bool, "yes", false, false );
		CHECK_FROM_STRING( bool, "maybe", false, false );
		CHECK_FROM_STRING( std::string_view, "abc", "abc", true );
	}

	TEST_CASE( "to_chars" )
	{
		CHECK_TO_CHARS( bool, true, "true" );
		CHECK_TO_CHARS( bool, false, "false" );
		CHECK_TO_CHARS( int, 12345, "12345" );
		CHECK_TO_CHARS( float, 3.14f, "3.14" );
		CHECK_TO_CHARS( double, 2.71828, "2.71828" );
		CHECK_TO_CHARS( const char *, "Hello, world!", "\"Hello, world!\"" );
		CHECK_TO_CHARS( std::string_view, "Test string", "\"Test string\"" );
		CHECK_TO_CHARS( std::string_view, "xxx \"quotes\" yyy", "'xxx \"quotes\" yyy'" );

		using array_t = std::array<int, 3>;
		array_t arr = { 1, 2, 3 };
		CHECK_TO_CHARS( array_t, arr, "{1 2 3}" );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Simple setter" )
{
	int value = 1;

	void set( int x )
	{
		value = x;
	}

	TEST_CASE( "Simple setter" )
	{
		const conco::command commands[] = {
			{ set, "set;Set value" },
		};

		REQUIRE( value == 1 );
		CHECK( execute( commands, "set 666" ) == conco::result::success );
		CHECK( execute( commands, "xset 123" ) == conco::result::command_not_found );
		REQUIRE( value == 666 );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Capture results" )
{
	int sum( int x, int y )
	{
		return x + y;
	}

	const char *c_str()
	{
		return "Hello!";
	}

	TEST_CASE( "Capture results" )
	{
		const conco::command commands[] = {
			{ sum, "sum;Sum of two values" },
			{ +[]( int x, int y ) { return x * y; }, "mul;Multiply two values" },
			{ &c_str, "c_str;Return C string" },
		};

		char buffer[64] = { 0 };

		CHECK( execute( commands, "sum 123 456", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "579" );

		CHECK( execute( commands, "mul 12 34", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "408" );

		CHECK( execute( commands, "c_str", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "\"Hello!\"" );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Callback types" )
{
	struct calculator
	{
		int add( int x, int y ) { return x + y; }
		int sub( int x, int y ) { return x - y; }
		int const_foo() const { return 123; }
	};

	TEST_CASE( "Member function commands" )
	{
		calculator calc;

		const calculator &const_calc = calc;

		const conco::command commands[] = {
			conco::method<&calculator::add>( calc, "add x y" ),
			conco::method<&calculator::sub>( calc, "sub x y" ),
			// conco::method<&calculator::sub>( const_calc, "sub x y" ), // Uncomment: this should not compile
			conco::method<&calculator::const_foo>( calc, "const_foo" ),
			conco::method<&calculator::const_foo>( const_calc, "const_foo_const_instance" ),
		};

		CHECK( commands[0].desc.arg_count == 2 );
		CHECK( commands[0].desc.command_arg_count == 2 );

		char buffer[64] = { 0 };

		CHECK( execute( commands, "add 100 250", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "350" );

		CHECK( execute( commands, "sub 500 123", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "377" );
	}

	TEST_CASE( "Lambda commands" )
	{
		int value = 42;

		auto capturing_lambda = [&]( int x, int y ) {
			value = x + y;
			return value;
		};

		const conco::command commands[] = {
			{ +[]( int x, int y ) { return x + y; }, "add x y" },
			{ +[]( int x, int y ) { return x - y; }, "sub x y" },
			{ capturing_lambda, "add_capture x y" },
		};

		CHECK( commands[0].desc.arg_count == 2 );
		CHECK( commands[0].desc.command_arg_count == 2 );

		char buffer[64] = { 0 };
		CHECK( execute( commands, "add 100 250;", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "350" );

		CHECK( execute( commands, "sub 500 123", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "377" );

		CHECK( execute( commands, "add_capture 10 20", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "30" );
	}

	struct callable_struct
	{
		int operator()( int x, int y ) { return x * y; }
	};

	TEST_CASE( "Callable" )
	{
		callable_struct multiplier;

		const conco::command commands[] = {
			{ multiplier, "mul x y" },
		};

		CHECK( commands[0].desc.arg_count == 2 );
		CHECK( commands[0].desc.command_arg_count == 2 );

		char buffer[64] = { 0 };
		CHECK( execute( commands, "mul 12 34", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "408" );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Default arguments" )
{
	TEST_CASE( "Default arguments" )
	{
		const conco::command commands[] = {
			{ +[]( int x, int y, int z, int w ) { return x + y + z + w; },
			  "bar x=1 y = 2 z= 3 w =4;Compute sum of four integers" },
		};

		char buffer[64] = { 0 };
		CHECK( execute( commands, "bar", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "10" );

		CHECK( execute( commands, "bar 10", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "19" );

		CHECK( execute( commands, "bar 10 20", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "37" );

		CHECK( execute( commands, "bar 10 20 30", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "64" );

		CHECK( execute( commands, "bar 10 20 30 40", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "100" );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Overloading" )
{
	TEST_CASE( "Overloading" )
	{
		const conco::command commands[] = {
			{ +[]( int x, int y ) { return x + y; }, "compute x y=100;Compute sum of two integers" },
			{ +[]( std::string_view str ) { return str.size(); }, "compute;Compute length of a string" },
		};

		char buffer[64] = { 0 };

		CHECK( execute( commands, "compute 10 20", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "30" );

		CHECK( execute( commands, "compute 10", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "110" );

		CHECK( execute( commands, "compute HelloWorld", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "10" );

		CHECK( execute( commands, "compute", buffer ) == conco::result::no_matching_overload );
	}

	TEST_CASE( "Overloading errors" )
	{
		const conco::command commands[] = {
			{ +[]( int x, int y, int z, int w ) { return x + y + z + w; }, "overload" },
			{ +[]( int x, int y, int z ) { return x + y + z; }, "overload" },
			{ +[]( int x, int y ) { return x + y; }, "overload" },
		};

		CHECK( execute( commands, "overload 10" ) == conco::result::no_matching_overload );
		CHECK( execute( commands, "overload a b c d" ) == conco::result::no_matching_overload );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Tail arguments" )
{
	int sum_all( conco::tokenizer & args )
	{
		int out = 0;

		std::optional<std::string_view> token;
		while ( ( token = args.next() ).has_value() )
		{
			int value = 0;
			if ( auto value_opt = conco::from_string( conco::tag<int>{}, *token ); value_opt.has_value() )
				value = *value_opt;

			out += value;
		}

		return out;
	}

	TEST_CASE( "Tail arguments" )
	{
		const conco::command commands[] = { { sum_all, "sum_all;Sum all arguments" } };

		char buffer[64] = { 0 };

		CHECK( execute( commands, "sum_all 1 2 3 4 5", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "15" );

		CHECK( commands[0].desc.has_tail_args == true );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "std::optional<T>" )
{
	int foo( std::optional<int> x )
	{
		return x.value_or( 42 );
	}

	TEST_CASE( "std::optional<T>" )
	{
		const conco::command commands[] = {
			{ foo, "foo x;Return the value of x or 42 if not provided" },
		};

		char buffer[64] = { 0 };
		CHECK( execute( commands, "foo 100", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "100" );

		CHECK( execute( commands, "foo", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "42" );
	}

	TEST_CASE( "Optional result" )
	{
		const conco::command commands[] = {
			{ +[]( int x ) -> std::optional<int> {
			   if ( x % 2 == 0 )
				   return x / 2;
			   else
				   return std::nullopt;
			 },
			  "half_if_even x;Return half of x if it's even, otherwise no result" },
		};

		char buffer[64] = { 0 };
		CHECK( execute( commands, "half_if_even 100", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "50" );

		CHECK( execute( commands, "half_if_even 33", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "" ); // No result
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Error handling" )
{
	int divide( int x, int y )
	{
		return x / y;
	}

	TEST_CASE( "Result" )
	{
		const conco::command commands[] = {
			{ divide, "divide;Divide two integers" },
		};

		char buffer[64] = { 0 };
		conco::output out = { buffer };

		CHECK( execute( commands, "divide 100 20", out ) == conco::result::success );
		REQUIRE( out.cmd != nullptr );
		REQUIRE( out.arg_error_mask == 0 );
		REQUIRE( out.result_error == false );
		REQUIRE( std::string_view( buffer ) == "5" );

		CHECK( execute( commands, "xxxdivide 100 20", out ) == conco::result::command_not_found );
		CHECK( execute( commands, "divide 100", out ) == conco::result::not_enough_arguments );
		CHECK( execute( commands, "divide 100 'LOL'", out ) == conco::result::argument_parsing_error );
	}

	int throwing_divide( int x, int y )
	{
		if ( y == 0 )
			throw std::runtime_error( "Division by zero" );

		return x / y;
	}

	TEST_CASE( "Exceptions" )
	{
		const conco::command commands[] = {
			{ throwing_divide, "divide;Divide two integers" },
		};

		char buffer[64] = { 0 };
		conco::output out = { buffer };

		try
		{
			execute( commands, "divide 100 0", out );
			REQUIRE( false ); // Should not get there
		}
		catch ( std::runtime_error & )
		{
			REQUIRE( true );
		}
		catch ( ... )
		{
			REQUIRE( false ); // Should not get there either!
		}
	}

	TEST_CASE( "Argument parsing errors" )
	{
		const conco::command commands[] = {
			{ +[]( int x, int y, int z, int w ) {}, "foo" },
		};

		char buffer[64] = { 0 };
		conco::output out = { buffer };

		CHECK( execute( commands, "foo 1 2 3 4", out ) == conco::result::success );
		REQUIRE( out.arg_error_mask == 0 );

		CHECK( execute( commands, "foo abc 2 3 4", out ) == conco::result::argument_parsing_error );
		REQUIRE( out.arg_error_mask == 0b0001 );

		CHECK( execute( commands, "foo 1 abc 3 4", out ) == conco::result::argument_parsing_error );
		REQUIRE( out.arg_error_mask == 0b0010 );

		CHECK( execute( commands, "foo 1 2 abc 4", out ) == conco::result::argument_parsing_error );
		REQUIRE( out.arg_error_mask == 0b0100 );

		CHECK( execute( commands, "foo 1 2 3 abc", out ) == conco::result::argument_parsing_error );
		REQUIRE( out.arg_error_mask == 0b1000 );

		CHECK( execute( commands, "foo abc 2 abc 4", out ) == conco::result::argument_parsing_error );
		REQUIRE( out.arg_error_mask == 0b0101 );

		CHECK( execute( commands, "foo 1 abc 3 abc", out ) == conco::result::argument_parsing_error );
		REQUIRE( out.arg_error_mask == 0b1010 );
	}

	TEST_CASE( "Small result buffer" )
	{
		const conco::command commands[] = {
			{ +[]( int x, int y ) { return x + y; }, "sum;Sum two integers" },
		};

		char buffer[2] = { 0 }; // Too small to hold any result
		conco::output out = { buffer };
		CHECK( execute( commands, "sum 100 200", out ) == conco::result::success );
		REQUIRE( out.result_error == true );

		char still_too_small_buffer[3] = { 0 }; // Can hold "300" but not null-terminator
		out = { still_too_small_buffer };
		CHECK( execute( commands, "sum 100 200", out ) == conco::result::success );
		REQUIRE( out.result_error == true );

		char exact_buffer[4] = { 0 }; // Just enough to hold "300" and null-terminator
		out = { exact_buffer };
		CHECK( execute( commands, "sum 100 200", out ) == conco::result::success );
		REQUIRE( out.result_error == false );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct point
{
	int x, y;
};

constexpr std::string_view type_name( conco::tag<point> ) noexcept
{
	return "point";
}

std::optional<point> from_string( conco::tag<point>, std::string_view str ) noexcept
{
	conco::tokenizer tokenizer{ str };

	auto tx = tokenizer.next();
	auto ty = tokenizer.next();
	if ( !tx || !ty )
		return std::nullopt;

	auto ox = conco::from_string( conco::tag<decltype( point::x )>{}, *tx );
	auto oy = conco::from_string( conco::tag<decltype( point::y )>{}, *ty );
	if ( !ox || !oy )
		return std::nullopt;

	return point{ *ox, *oy };
}

bool to_chars( conco::tag<point>, std::span<char> buffer, point p ) noexcept
{
	auto r = std::format_to_n( buffer.data(), buffer.size(), "{{{} {}}}", p.x, p.y );
	*r.out = '\0';
	return true;
}

TEST_SUITE( "Custom types" )
{
	point add_points( point p1, point p2 )
	{
		return point{ p1.x + p2.x, p1.y + p2.y };
	}

	TEST_CASE( "Custom type" )
	{
		const conco::command commands[] = {
			{ add_points, "add_points p1 p2={10 20};Add two points" },
		};

		char buffer[64] = { 0 };
		CHECK( execute( commands, "add_points {1 2} {3 4}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{4 6}" );

		CHECK( execute( commands, "add_points {5 6}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{15 26}" );
	}
}

TEST_SUITE( "Structured bindings" )
{
	using sum_1 = std::tuple<int>;
	using sum_2 = std::tuple<int, int>;
	using sum_3 = std::tuple<int, int, int>;
	using sum_4 = std::tuple<int, int, int, int>;

	using pair = std::pair<int, int>;

	TEST_CASE( "Different number of members" )
	{
		const conco::command commands[] = {
			{ +[]( sum_1 f ) { return std::get<0>( f ); }, "sum_1" },
			{ +[]( sum_2 f ) { return std::get<0>( f ) + std::get<1>( f ); }, "sum_2" },
			{ +[]( sum_3 f ) { return std::get<0>( f ) + std::get<1>( f ) + std::get<2>( f ); }, "sum_3" },
			{ +[]( sum_4 f ) { return std::get<0>( f ) + std::get<1>( f ) + std::get<2>( f ) + std::get<3>( f ); }, "sum_4" },
			{ +[]( pair p ) { return p.first + p.second; }, "sum_pair" },
		};

		char buffer[64] = { 0 };
		CHECK( execute( commands, "sum_1 5", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "5" );

		CHECK( execute( commands, "sum_2 {5 10}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "15" );

		CHECK( execute( commands, "sum_3 {5 10 15}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "30" );

		CHECK( execute( commands, "sum_4 {5 10 15 20}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "50" );

		CHECK( execute( commands, "sum_4 {5 10 15}", buffer ) == conco::result::argument_parsing_error );

		CHECK( execute( commands, "sum_pair {7 8}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "15" );
	}

	TEST_CASE( "Results" )
	{
		const conco::command commands[] = {
			{ +[]( int x ) { return sum_1{ x }; }, "make_sum_1 x" },
			{ +[]( int x, int y ) { return sum_2{ x, y }; }, "make_sum_2 x y" },
			{ +[]( int x, int y, int z ) { return sum_3{ x, y, z }; }, "make_sum_3 x y z" },
			{ +[]( int x, int y, int z, int w ) { return sum_4{ x, y, z, w }; }, "make_sum_4 x y z w" },
			{ +[]( int x, int y ) { return pair{ x, y }; }, "make_pair x y" },
		};

		char buffer[64] = { 0 };
		CHECK( execute( commands, "make_sum_1 42", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{42}" );

		CHECK( execute( commands, "make_sum_2 10 20", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{10 20}" );

		CHECK( execute( commands, "make_sum_3 1 2 3", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{1 2 3}" );

		CHECK( execute( commands, "make_sum_4 4 3 2 1", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{4 3 2 1}" );

		CHECK( execute( commands, "make_pair 7 8", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{7 8}" );
	}
}

TEST_SUITE( "STL types" )
{
	TEST_CASE( "std::vector" )
	{
		const conco::command commands[] = {
			{ +[]( const std::vector<int> &vec ) -> int {
			   int sum = 0;
			   for ( auto v : vec )
				   sum += v;
			   return sum;
			 },
			  "sum_vector vec;Sum all integers in the vector" },

			{ +[]( conco::tokenizer &args ) -> std::vector<int> {
			   std::vector<int> vec;

			   conco::token t;
			   while ( ( t = args.next() ).has_value() )
			   {
				   if ( auto value_opt = conco::from_string( conco::tag<int>{}, *t ); value_opt.has_value() )
					   vec.push_back( *value_opt );
			   }

			   return vec;
			 },
			  "make_vector;Create a vector of integers from the provided arguments" },
		};

		char buffer[64] = { 0 };
		CHECK( execute( commands, "sum_vector {1 2 3 4 5}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "15" );

		CHECK( execute( commands, "sum_vector {}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "0" );

		CHECK( execute( commands, "make_vector 10 20 30 40", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{10 20 30 40}" );
	}

	TEST_CASE( "std::map" )
	{
		const conco::command commands[] = {
			{ +[]( const std::map<std::string, int> &m ) -> std::pair<std::string, int> {
			   std::pair<std::string, int> result = {};
			   for ( const auto &[key, value] : m )
			   {
				   result.first += key;
				   result.second += value;
			   }

			   return result;
			 },
			  "sum_map m;Sum all keys and values in the map" },
			{ +[]( conco::tokenizer &args ) -> std::map<std::string, int> {
			   std::map<std::string, int> m;

			   while ( true )
			   {
				   auto key = args.next();
				   if ( !key )
					   break;

				   if ( !args.consume_char_if( '=' ) )
					   break;

				   auto value = args.next();
				   if ( !value )
					   break;

				   auto parsed_key_opt = conco::from_string( conco::tag<std::string>{}, *key );
				   auto parsed_value_opt = conco::from_string( conco::tag<int>{}, *value );

				   if ( parsed_key_opt && parsed_value_opt )
					   m.emplace( *parsed_key_opt, *parsed_value_opt );
				   else
					   break;
			   }

			   return m;
			 },
			  "make_map;Create a map from key=value pairs" },
		};

		char buffer[64] = { 0 };
		CHECK( execute( commands, "sum_map {a=10 b=20 c=30}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{\"abc\" 60}" );
		CHECK( execute( commands, "sum_map {}", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{\"\" 0}" );
		CHECK( execute( commands, "make_map key1=100 key2=200 key3=300 'key X'=400", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "{\"key X\"=400 \"key1\"=100 \"key2\"=200 \"key3\"=300}" );
	}
}
