#pragma once

#include <optional>
#include <string_view>

namespace conco {

using token = std::optional<std::string_view>;

/**
 * Very simple tokenizer that splits a string into a substrings (tokens) based on whitespace, commas
 * or semicolons. Single and double quotes are supported to allow longer tokens with spaces.
 * Enclosing and nesting tokens between brackets {} is also supported.
 *
 * Examples:
 *   "a b, c;d,e f"      -> "a", "b", "c", "d", "e", "f"
 *   "'a b c d e f'"     -> "a b c d e f"
 *   "a 'b c';d"         -> "a", "b c", "d"
 *   "a {b c {d e} f} g" -> "a", "b c {d e} f", "g"
 *   "a {b '{c d}' e} f" -> "a", "b '{c d}' e", "f"
 *
 * Special characters:
 *   ';' -> indicates end of command, tokenizer will stop processing further input
 *   '=' -> single-character token, returned as-is (used for key-value assignments)
 *   '{' -> starts a block token, ends at matching '}'
 *   '"' or ''' -> starts a quoted string token, ends at matching quote
 *   '\' -> backslash escape - next character is treated literally
 *
 * When the tokenizer is unable to produce a valid token, it returns `std::nullopt`.
 * This can happen because:
 *   - input `text` string is empty
 *   - quotes or brackets are not properly closed
 *   - semicolon is encountered (indicates end of command)
 *
 * Important note: the tokenizer is "zero-copy", meaning that it does not allocate or copy anything.
 * It will always produce string views into the original input string. Therefore, if there are any
 * escape sequences (like `\'` or `\"`), the backslash character will be preserved in the output
 * and it's up to the user to handle it appropriately.
 */
struct tokenizer
{
	// Characters that are considered whitespace or delimiters between tokens
	static constexpr auto is_whitespace = []( char ch ) static -> bool { return ch <= ' ' || ch == ','; };

	// Characters that terminate identifier token
	static constexpr auto is_ident_term = []( char ch ) static -> bool {
		return is_whitespace( ch ) || ch == ';' || ch == '=' || ch == '"' || ch == '\'' || ch == '{' || ch == '}';
	};

	std::string_view text;

	tokenizer( const tokenizer & ) = default;
	tokenizer( tokenizer && ) = default;

	tokenizer( std::string_view str ) : text( str ) { consume_whitespace(); }

	bool empty() const noexcept { return text.empty(); }

	void reset( std::string_view str )
	{
		text = str;
		consume_whitespace();
	}

	bool next_char_is( char ch ) const noexcept { return !text.empty() && text[0] == ch; }

	void consume_whitespace()
	{
		size_t i = 0;
		while ( i < text.size() && is_whitespace( text[i] ) )
			++i;

		text.remove_prefix( i );
	}

	bool consume_char_if( char ch ) noexcept
	{
		if ( next_char_is( ch ) )
		{
			text.remove_prefix( 1 );
			consume_whitespace();
			return true;
		}

		return false;
	}

	token next() noexcept
	{
		if ( text.empty() )
			return std::nullopt;

		switch ( text[0] )
		{
			case '"':
			case '\'': return parse_quoted_string();

			case '{': return parse_block();
			case '=': return extract_token( 1, false );
			case ';': return std::nullopt;

			default: break;
		}

		return parse_identifier();
	}

private:
	token extract_token( size_t length, bool trim_ends )
	{
		if ( length > text.size() )
		{
			text = {};
			return std::nullopt;
		}

		std::string_view token = trim_ends ? text.substr( 1, length - 2 ) : text.substr( 0, length );
		text.remove_prefix( length );

		consume_whitespace();
		return token;
	}

	token parse_identifier()
	{
		char prev_ch = '\0';

		size_t i = 0;
		while ( i < text.size() )
		{
			char ch = text[i];

			if ( prev_ch != '\\' )
			{
				if ( is_ident_term( ch ) )
					break;
			}
			else if ( ch == '\\' ) // Escaped backslash
				ch = '\0';

			prev_ch = ch;
			++i;
		}

		return extract_token( i, false );
	}

	token parse_quoted_string()
	{
		char prev_ch = '\0';
		char quote_char = text[0];

		size_t i = 1;
		while ( i < text.size() )
		{
			char ch = text[i];

			if ( prev_ch != '\\' )
			{
				if ( ch == quote_char )
				{
					quote_char = '\0';
					++i;
					break;
				}
			}
			else if ( ch == '\\' ) // Escaped backslash
				ch = '\0';

			prev_ch = ch;
			++i;
		}

		if ( quote_char )
		{
			text = {};
			return std::nullopt;
		}

		return extract_token( i, true );
	}

	token parse_block()
	{
		size_t depth = 1;
		char prev_ch = '\0';
		char quote_char = '\0';

		size_t i = 1;
		while ( i < text.size() && depth > 0 )
		{
			char ch = text[i];

			if ( prev_ch != '\\' )
			{
				if ( quote_char )
				{
					if ( ch == quote_char )
						quote_char = '\0';
				}
				else
				{
					if ( ch == '"' || ch == '\'' )
						quote_char = ch;
					else if ( ch == ';' )
						break;
					else if ( ch == '{' )
						++depth;
					else if ( ch == '}' && !( --depth ) )
					{
						++i;
						break;
					}
				}
			}
			else if ( ch == '\\' )
				ch = '\0';

			prev_ch = ch;
			++i;
		}

		if ( depth > 0 )
		{
			text = {};
			return std::nullopt;
		}

		return extract_token( i, true );
	}
};

} // namespace conco
