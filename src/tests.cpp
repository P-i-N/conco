#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/parts/doctest.cpp>

#include "conco/conco.hpp"

#include <print>

namespace {

conco::result execute( std::span<const conco::command> commands,
                       const char *cmd_line,
                       conco::output &out,
                       void *user_data = nullptr )
{
	std::string input_buffer = cmd_line ? cmd_line : "";
	return conco::execute( commands, input_buffer.data(), out, user_data );
}

conco::result execute( std::span<const conco::command> commands,
                       const char *cmd_line,
                       std::span<char> output_buffer = {},
                       void *user_data = nullptr )
{
	conco::output out = { output_buffer };
	return execute( commands, cmd_line, out, user_data );
}

} // namespace

#define CHECK_NEXT_TOKEN( _Value, _Count ) \
	do \
	{ \
		auto token = tokenizer.next(); \
		REQUIRE( token.has_value() ); \
		REQUIRE( *token == _Value ); \
		REQUIRE( tokenizer.count == _Count ); \
	} while ( false )

#define CHECK_NEXT_EMPTY_TOKEN( _Count ) \
	do \
	{ \
		auto token = tokenizer.next(); \
		REQUIRE( !token.has_value() ); \
		REQUIRE( tokenizer.count == _Count ); \
	} while ( false )

TEST_SUITE( "Tokenizer tests" )
{
	TEST_CASE( "Empty string" )
	{
		std::string input = "";
		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };
		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_EMPTY_TOKEN( 0 );
	}

	TEST_CASE( "Basic split test" )
	{
		std::string input = R"(    first_token"second token", 'third token';    fourth_token "'5th'" '"6th')";
		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };
		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_TOKEN( "first_token", 1 );
		CHECK_NEXT_TOKEN( "second token", 2 );
		CHECK_NEXT_TOKEN( "third token", 3 );
		CHECK_NEXT_TOKEN( "fourth_token", 4 );
		CHECK_NEXT_TOKEN( "'5th'", 5 );
		CHECK_NEXT_TOKEN( "\"6th", 6 );

		CHECK_NEXT_EMPTY_TOKEN( 6 );
	}

	TEST_CASE( "Unclosed string" )
	{
		std::string input = R"(    "unclosed string )";
		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };
		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_EMPTY_TOKEN( 0 );
	}

	TEST_CASE( "Escaping" )
	{
		std::string input = R"("X\"")";
		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };
		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_TOKEN( "X\\\"", 1 );
		CHECK_NEXT_EMPTY_TOKEN( 1 );
	}

	TEST_CASE( "Brackets test" )
	{
		std::string input = R"(token1 {token2} {token3;token3} {})";
		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };
		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_TOKEN( "token1", 1 );
		CHECK_NEXT_TOKEN( "token2", 2 );
		CHECK_NEXT_TOKEN( "token3;token3", 3 );
		CHECK_NEXT_TOKEN( "", 4 );

		CHECK_NEXT_EMPTY_TOKEN( 4 );
	}

	TEST_CASE( "Nested brackets test" )
	{
		std::string input = R"(token1 {token2 {token3}, token2_end}, token4)";
		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };
		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_TOKEN( "token1", 1 );
		CHECK_NEXT_TOKEN( "token2 {token3}, token2_end", 2 );
		CHECK_NEXT_TOKEN( "token4", 3 );

		CHECK_NEXT_EMPTY_TOKEN( 3 );
	}

	TEST_CASE( "One big nest" )
	{
		std::string input = R"({{nested {brackets {1 {2 {3 {4}}}}} test} inside})";
		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };
		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_TOKEN( "{nested {brackets {1 {2 {3 {4}}}}} test} inside", 1 );

		CHECK_NEXT_EMPTY_TOKEN( 1 );
	}

	TEST_CASE( "Unclosed brackets" )
	{
		std::string input = R"(token1 {token2 {token3} token2_end token4)";
		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };
		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_TOKEN( "token1", 1 );

		CHECK_NEXT_EMPTY_TOKEN( 1 );
	}

	TEST_CASE( "Mixed tokens" )
	{
		std::string input = R"(
			first_token
			"second token"
			"third token with {braces}"
			{fourth_token with 'quotes', {braces} and "quoted {braces}"}
			fifth_token
			)";

		conco::tokenizer tokenizer{ { input.data(), input.size() + 1 } };

		REQUIRE( tokenizer.count == 0 );

		CHECK_NEXT_TOKEN( "first_token", 1 );
		CHECK_NEXT_TOKEN( "second token", 2 );
		CHECK_NEXT_TOKEN( "third token with {braces}", 3 );
		CHECK_NEXT_TOKEN( "fourth_token with 'quotes', {braces} and \"quoted {braces}\"", 4 );
		CHECK_NEXT_TOKEN( "fifth_token", 5 );

		CHECK_NEXT_EMPTY_TOKEN( 5 );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CHECK_TYPE_PARSER_FROM_STRING( _Type, _Str, _ExpectedValue, _HasValue ) \
	do \
	{ \
		auto opt = conco::type_parser<_Type>::from_string( _Str ); \
		REQUIRE( opt.has_value() == _HasValue ); \
		if ( _HasValue ) \
			REQUIRE( *opt == _ExpectedValue ); \
	} while ( false )

TEST_SUITE( "Type parsing tests" )
{
	TEST_CASE( "Basic types" )
	{
		/*
		CHECK_TYPE_PARSER_FROM_STRING( int, "123", 123, true );
		CHECK_TYPE_PARSER_FROM_STRING( int, "0x123", 0x123, true );
		CHECK_TYPE_PARSER_FROM_STRING( int, "0b11001010", 0b11001010, true );
		CHECK_TYPE_PARSER_FROM_STRING( int, "abc", 0, false );
		CHECK_TYPE_PARSER_FROM_STRING( bool, "true", true, true );
		CHECK_TYPE_PARSER_FROM_STRING( bool, "false", false, true );
		CHECK_TYPE_PARSER_FROM_STRING( bool, "1", true, true );
		CHECK_TYPE_PARSER_FROM_STRING( bool, "0", false, true );
		CHECK_TYPE_PARSER_FROM_STRING( bool, "yes", false, false );
		CHECK_TYPE_PARSER_FROM_STRING( bool, "maybe", false, false );
		CHECK_TYPE_PARSER_FROM_STRING( std::string_view, "abc", "abc", true );
		*/
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
		static const conco::command commands[] = {
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
		static const conco::command commands[] = {
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
		REQUIRE( std::string_view( buffer ) == "Hello!" );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Pass user data" )
{
	int mul( int x, int y, const void *user_data )
	{
		int factor = *reinterpret_cast<const int *>( user_data );
		return x * y * factor;
	}

	TEST_CASE( "Pass user data" )
	{
		static const conco::command commands[] = {
			{ mul, "mul;Multiply two values with factor" },
		};

		CHECK( commands[0].desc.arg_count == 3 );
		CHECK( commands[0].desc.command_arg_count == 2 );

		char buffer[64] = { 0 };

		int factor = 3;
		CHECK( execute( commands, "mul 10 20", buffer, &factor ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "600" ); // 10 * 20 * 3 = 600
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

		static const conco::command commands[] = {
			{ +[]( int x, int y ) { return x + y; }, "add x y" }, { +[]( int x, int y ) { return x - y; }, "sub x y" },
			//{ capturing_lambda, "capture_add x y" },
		};

		CHECK( commands[0].desc.arg_count == 2 );
		CHECK( commands[0].desc.command_arg_count == 2 );

		char buffer[64] = { 0 };
		CHECK( execute( commands, "add 100 250", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "350" );

		CHECK( execute( commands, "sub 500 123", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "377" );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_SUITE( "Overloading" )
{
	TEST_CASE( "Overloading" )
	{
		static const conco::command commands[] = {
			{ +[]( int x, int y ) { return x + y; }, "compute;Compute sum of two integers" },
			{ +[]( std::string_view str ) { return str.size(); }, "compute;Compute length of a string" },
		};

		char buffer[64] = { 0 };

		CHECK( execute( commands, "compute 10 20", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "30" );

		CHECK( execute( commands, "compute HelloWorld", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "10" );
	}

	TEST_CASE( "Overloading errors" )
	{
		static const conco::command commands[] = {
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

		std::optional<conco::token> token;
		while ( ( token = args.next() ).has_value() )
		{
			int value = 0;
			if ( auto value_opt = conco::type_parser<int>::from_chars( *token ); value_opt.has_value() )
				value = *value_opt;

			out += value;
		}

		return out;
	}

	TEST_CASE( "Tail arguments" )
	{
		static const conco::command commands[] = { { sum_all, "sum_all;Sum all arguments" } };

		char buffer[64] = { 0 };

		CHECK( execute( commands, "sum_all 1 2 3 4 5", buffer ) == conco::result::success );
		REQUIRE( std::string_view( buffer ) == "15" );

		CHECK( commands[0].desc.has_tail_args == true );
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
		static const conco::command commands[] = {
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
		static const conco::command commands[] = {
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
}
