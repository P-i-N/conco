#pragma once

#include "conco.hpp"

#include <algorithm>
#include <concepts>
#include <charconv>

namespace conco {

template <> struct type_parser<bool>
{
	static constexpr std::string_view name = "bool";

	static std::optional<bool> from_string( std::string_view str )
	{
		if ( str == "true" || str == "1" )
			return true;
		else if ( str == "false" || str == "0" )
			return false;

		return std::nullopt;
	}

	static bool to_string( bool value, std::span<char> buff )
	{
		const char *str = value ? "true" : "false";
		size_t len = std::char_traits<char>::length( str );
		if ( buff.size() < len + 1 ) // +1 for null-terminator
			return false;

		std::copy_n( str, len + 1, buff.data() ); // Including null-terminator
		return true;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
  requires std::is_integral_v<T>
struct type_parser<T>
{
	static constexpr std::string_view name = "int";

	static std::optional<T> from_string( std::string_view str )
	{
		int base = 10;
		T out = 0;

		if ( str.starts_with( "0x" ) || str.starts_with( "0X" ) )
		{
			base = 16;
			str.remove_prefix( 2 );
		}
		else if ( str.starts_with( "0b" ) || str.starts_with( "0B" ) )
		{
			base = 2;
			str.remove_prefix( 2 );
		}

		auto r = std::from_chars( str.data(), str.data() + str.size(), out, base );
		if ( r.ec != std::errc() )
			return std::nullopt;

		return out;
	}

	static bool to_string( T value, std::span<char> buff )
	{
		auto r = std::to_chars( buff.data(), buff.data() + buff.size() - 1, value ); // Leave space for null-terminator
		if ( r.ec != std::errc() )
			return false;

		r.ptr[0] = '\0';
		return true;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <> struct type_parser<std::string_view>
{
	static constexpr std::string_view name = "string";

	static std::optional<std::string_view> from_string( std::string_view str ) { return str; }

	static bool to_string( std::string_view value, std::span<char> buff )
	{
		if ( buff.size() < value.size() + 1 ) // +1 for null-terminator
			return false;

		std::copy_n( value.data(), value.size(), buff.data() );
		buff[value.size()] = '\0';
		return true;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <> struct type_parser<const char *>
{
	static constexpr std::string_view name = "string";

	static bool to_string( const char *value, std::span<char> buff )
	{
		size_t len = std::char_traits<char>::length( value );
		if ( buff.size() < len + 1 ) // +1 for null-terminator
			return false;

		std::copy_n( value, len + 1, buff.data() ); // Including null-terminator
		return true;
	}
};

} // namespace conco
