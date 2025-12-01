#pragma once

#include <optional>
#include <string_view>

namespace conco {

/**
 * Very simple tokenizer that splits a string into a substrings (tokens) based on whitespace, commas
 * or semicolons. Single and double quotes are supported to allow tokens with spaces. Enclosing and
 * nesting tokens between brackets {} is also supported.
 *
 * Examples:
 *   "a b, c;d,e f"      -> "a", "b", "c", "d", "e", "f"
 *   "'a b c d e f'"     -> "a b c d e f"
 *   "a 'b c';d"         -> "a", "b c", "d"
 *   "a {b c {d e} f} g" -> "a", "b c {d e} f", "g"
 *   "a {b '{c d}' e} f" -> "a", "b '{c d}' e", "f"
 */
struct tokenizer
{
	std::string_view text;
	size_t count = 0;

	// Characters that terminate identifier token
	static constexpr auto is_ident_term = []( char ch ) static -> bool {
		return ch <= ' ' || ch == ',' || ch == ';' || ch == '"' || ch == '\'' || ch == '{' || ch == '}';
	};

	// Characters that are considered whitespace or delimiters between tokens
	static constexpr auto is_whitespace = []( char ch ) static -> bool { return ch <= ' ' || ch == ',' || ch == ';'; };

	void consume_whitespace()
	{
		size_t i = 0;
		while ( i < text.size() && is_whitespace( text[i] ) )
			++i;

		text.remove_prefix( i );
	}

	std::optional<std::string_view> next() noexcept
	{
		consume_whitespace();

		if ( text.empty() )
			return std::nullopt;

		auto first = text[0];

		if ( first == '"' || first == '\'' )
			return parse_quoted_string();

		if ( first == '{' )
			return parse_block();

		return parse_identifier();
	}

private:
	std::optional<std::string_view> extract_token( size_t length, bool trim_ends )
	{
		if ( length > text.size() )
		{
			text = {};
			return std::nullopt;
		}

		std::string_view token = trim_ends ? text.substr( 1, length - 2 ) : text.substr( 0, length );
		text.remove_prefix( length );
		++count;

		return token;
	}

	std::optional<std::string_view> parse_identifier()
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

	std::optional<std::string_view> parse_quoted_string()
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

	std::optional<std::string_view> parse_block()
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
