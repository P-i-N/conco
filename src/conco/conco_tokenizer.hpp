#pragma once

#include <optional>
#include <string_view>

namespace conco {

/**
 * A lightweight token representation as a span of characters.
 *
 * The token is always null-terminated, so it can be used as a C-string directly.
 * The size() includes the null-terminator character.
 */
struct token : std::span<char>
{
	using std::span<char>::span;

	token() = default;
	token( std::nullptr_t ) noexcept : std::span<char>() {}
	token( std::span<char> other ) noexcept : std::span<char>( other ) {}

	bool operator==( std::string_view other ) const noexcept
	{
		if ( size() != other.size() + 1 )
			return false;

		return std::memcmp( data(), other.data(), size() ) == 0;
	}

	// Implicit conversion to std::string_view (excluding null-terminator!)
	operator std::string_view() const noexcept
	{
		return empty() ? std::string_view() : std::string_view( data(), size() - 1 );
	}

	bool valid() const noexcept { return data() && size() > 0; }

	// C-string access - safe, because null-terminator is always included
	const char *c_str() const noexcept { return data(); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
 *
 * The tokenizer modifies the input string by inserting null-terminators between tokens.
 * Expect the input string to be changed after using this tokenizer! This is a trade-off
 * for
 */
struct tokenizer
{
	token text;
	size_t count = 0;

	// Characters that terminate identifier token
	static constexpr auto is_ident_term = []( char ch ) static -> bool {
		return ch <= ' ' || ch == ',' || ch == ';' || ch == '"' || ch == '\'' || ch == '{' || ch == '}';
	};

	// Characters that are considered whitespace or delimiters between tokens
	static constexpr auto is_whitespace = []( char ch ) static -> bool { return ch <= ' ' || ch == ',' || ch == ';'; };

	tokenizer() = default;

	tokenizer( token t ) noexcept : text( t ) { _first_char = t.empty() ? '\0' : t[0]; }

	tokenizer( char *str ) noexcept : tokenizer( { str, str ? ( std::strlen( str ) + 1 ) : 0 } ) {}

	void consume_whitespace()
	{
		size_t i = 0;
		while ( i < text.size() && is_whitespace( at( i ) ) )
			++i;

		if ( i > 0 )
		{
			text = text.subspan( i );
			_first_char = text.empty() ? '\0' : text[0];
		}
	}

	std::optional<token> next() noexcept
	{
		consume_whitespace();

		if ( text.empty() )
			return std::nullopt;

		if ( _first_char == '"' || _first_char == '\'' )
			return parse_quoted_string();

		if ( _first_char == '{' )
			return parse_block();

		return parse_identifier();
	}

private:
	char _first_char = '\0';

	char at( size_t index ) const { return index == 0 ? _first_char : text[index]; }

	std::optional<token> extract_token( size_t token_length, size_t consume_length )
	{
		if ( token_length >= text.size() )
		{
			text = {};
			return std::nullopt;
		}

		_first_char = text[consume_length];

		text[token_length] = '\0';
		token t = text.subspan( 0, token_length + 1 );
		text = text.subspan( consume_length );
		++count;

		return t;
	}

	std::optional<token> parse_identifier()
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

		return extract_token( i, i );
	}

	std::optional<token> parse_quoted_string()
	{
		char prev_ch = '\0';
		char quote_char = _first_char;
		text = text.subspan( 1 ); // Consume opening quote

		size_t i = 0;
		while ( i < text.size() )
		{
			char ch = text[i];

			if ( prev_ch != '\\' )
			{
				if ( ch == quote_char )
				{
					quote_char = '\0';
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

		return extract_token( i, i + 1 );
	}

	std::optional<token> parse_block()
	{
		size_t depth = 1;
		char prev_ch = '\0';
		char quote_char = '\0';
		text = text.subspan( 1 ); // Consume opening brace

		size_t i = 0;
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
						break;
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

		return extract_token( i, i + 1 );
	}
};

} // namespace conco
