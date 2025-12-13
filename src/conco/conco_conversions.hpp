#pragma once

#include "conco.hpp"

#include <algorithm>
#include <concepts>
#include <charconv>
#include <optional>

namespace conco {

constexpr std::string_view type_name( tag<bool> ) noexcept
{
	return "bool";
}

std::optional<bool> from_string( tag<bool>, std::string_view str ) noexcept
{
	if ( !std::strcmp( str.data(), "true" ) || !std::strcmp( str.data(), "1" ) )
		return true;

	else if ( !std::strcmp( str.data(), "false" ) || !std::strcmp( str.data(), "0" ) )
		return false;

	return std::nullopt;
}

bool to_chars( tag<bool>, std::span<char> buff, bool value ) noexcept
{
	const char *str = value ? "true" : "false";
	size_t len = std::char_traits<char>::length( str );
	if ( buff.size() < len + 1 ) // +1 for null-terminator
		return false;

	std::copy_n( str, len + 1, buff.data() ); // Including null-terminator
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
  requires std::is_integral_v<T> && std::is_signed_v<T>
constexpr std::string_view type_name( tag<T> ) noexcept
{
	return "int";
}

template <typename T>
  requires std::is_integral_v<T>
std::optional<T> from_string( tag<T>, std::string_view str ) noexcept
{
	int base = 10;
	T out = 0;

	if ( str.size() >= 2 && str[0] == '0' )
	{
		if ( str[1] == 'x' || str[1] == 'X' )
		{
			base = 16;
			str.remove_prefix( 2 );
		}
		else if ( str[1] == 'b' || str[1] == 'B' )
		{
			base = 2;
			str.remove_prefix( 2 );
		}
	}

	auto r = std::from_chars( str.data(), str.data() + str.size(), out, base );
	if ( r.ec != std::errc() )
		return std::nullopt;

	return out;
}

template <typename T>
  requires std::is_integral_v<T>
bool to_chars( tag<T>, std::span<char> buff, T value ) noexcept
{
	auto r = std::to_chars( buff.data(), buff.data() + buff.size() - 1, value ); // Leave space for null-terminator
	if ( r.ec != std::errc() )
		return false;

	r.ptr[0] = '\0';
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::string_view type_name( tag<std::string_view> ) noexcept
{
	return "string";
}

std::optional<std::string_view> from_string( tag<std::string_view>, std::string_view str ) noexcept
{
	return str;
}

bool to_chars( tag<std::string_view>, std::span<char> buff, std::string_view value ) noexcept
{
	if ( buff.size() < value.size() + 1 ) // +1 for null-terminator
		return false;
	std::copy_n( value.data(), value.size(), buff.data() );
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::string_view type_name( tag<const char *> ) noexcept
{
	return "string";
}

// Note: There is no `from_string`, because we cannot return a null-terminated string from
// a source std::string_view without allocating memory for it.

bool to_chars( tag<const char *>, std::span<char> buff, const char *value ) noexcept
{
	size_t len = std::char_traits<char>::length( value );
	if ( buff.size() < len + 1 ) // +1 for null-terminator
		return false;

	std::copy_n( value, len + 1, buff.data() ); // Including null-terminator
	return true;
}

} // namespace conco
