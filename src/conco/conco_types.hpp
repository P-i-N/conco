#pragma once

#include "conco.hpp"

#include <array>
#include <concepts>
#include <charconv>

namespace conco::detail {

template <typename T> size_t to_chars_append( std::span<char> &buff, const T &value, bool leading_space ) noexcept
{
	if ( leading_space )
	{
		if ( buff.size() < 2 ) // Need at least space + null-terminator
			return 0;

		buff[0] = ' ';
		buff = buff.subspan( 1 );
	}

	size_t len = to_chars( tag<std::remove_cvref_t<T>>{}, buff, value );
	if ( len == 0 )
		return 0;

	buff = buff.subspan( len - 1 );
	return leading_space ? len : len - 1;
}

template <typename T>
concept is_tuple_like = requires { sizeof( std::tuple_size<T> ); };

template <typename T>
concept is_struct_bindable = std::is_array_v<std::remove_cvref_t<T>> || is_tuple_like<std::remove_cvref_t<T>> ||
                             std::is_aggregate_v<std::remove_cvref_t<T>>;

} // namespace conco::detail

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

size_t to_chars( tag<bool>, std::span<char> buff, bool value ) noexcept
{
	const char *str = value ? "true" : "false";
	size_t len = std::char_traits<char>::length( str );
	if ( buff.size() < len + 1 ) // +1 for null-terminator
		return 0;

	std::copy_n( str, len + 1, buff.data() ); // Including null-terminator
	return len + 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
  requires std::is_integral_v<T> && std::is_signed_v<T>
constexpr std::string_view type_name( tag<T> ) noexcept
{
	return "int";
}

template <typename T>
  requires std::is_integral_v<T> && std::is_unsigned_v<T>
constexpr std::string_view type_name( tag<T> ) noexcept
{
	return "uint";
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
  requires std::is_floating_point_v<T>
constexpr std::string_view type_name( tag<T> ) noexcept
{
	return "float";
}

template <typename T>
  requires std::is_floating_point_v<T>
std::optional<T> from_string( tag<T>, std::string_view str ) noexcept
{
	T out = 0;
	auto r = std::from_chars( str.data(), str.data() + str.size(), out );
	if ( r.ec != std::errc() )
		return std::nullopt;

	return out;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
  requires std::is_integral_v<T> || std::is_floating_point_v<T>
size_t to_chars( tag<T>, std::span<char> buff, T value ) noexcept
{
	auto r = std::to_chars( buff.data(), buff.data() + buff.size() - 1, value ); // Leave space for null-terminator
	if ( r.ec != std::errc() )
		return 0;

	r.ptr[0] = '\0';
	return r.ptr - buff.data() + 1;
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

size_t to_chars( tag<std::string_view>, std::span<char> buff, std::string_view value ) noexcept
{
	bool has_terminator = false;
	size_t num_double_quotes = 0;
	size_t num_single_quotes = 0;

	for ( char ch : value )
	{
		has_terminator |= tokenizer::is_ident_term( ch );
		num_double_quotes += ( ch == '"' ) ? 1 : 0;
		num_single_quotes += ( ch == '\'' ) ? 1 : 0;
	}

	if ( has_terminator || num_double_quotes > 0 || num_single_quotes > 0 )
	{
		size_t num_escapes = std::max( num_double_quotes, num_single_quotes );

		// +2 for enclosing quotes, +X for '\\', +1 for null-terminator
		if ( buff.size() < value.size() + 3 + num_escapes )
			return 0;

		char quote_char = ( num_double_quotes <= num_single_quotes ) ? '"' : '\'';
		buff[0] = quote_char;

		size_t i = 1;
		for ( char ch : value )
		{
			if ( ch == quote_char || ch == '\\' )
				buff[i++] = '\\';
			buff[i++] = ch;
		}

		buff[i++] = quote_char;
		buff[i++] = '\0';
		return i;
	}

	if ( buff.size() < value.size() + 1 ) // +1 for null-terminator
		return 0;

	std::copy_n( value.data(), value.size(), buff.data() );

	buff[value.size()] = '\0';
	return value.size() + 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::string_view type_name( tag<const char *> ) noexcept
{
	return "string";
}

// Note: There is no `from_string`, because we cannot return a null-terminated string from
// a source std::string_view token without allocating memory for it.

size_t to_chars( tag<const char *>, std::span<char> buff, const char *value ) noexcept
{
	return to_chars( tag<std::string_view>{}, buff, value ? std::string_view{ value } : std::string_view{} );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, size_t N> constexpr std::string_view type_name( tag<std::array<T, N>> ) noexcept
{
	return "array";
}

template <typename T, size_t N> constexpr std::string_view inner_type_name( tag<std::array<T, N>> ) noexcept
{
	return type_name( tag<T>{} );
}

template <typename T, size_t N> constexpr std::string_view type_name( tag<std::span<T, N>> ) noexcept
{
	return "span";
}

template <typename T> constexpr std::string_view inner_type_name( tag<std::span<T>> ) noexcept
{
	return type_name( tag<T>{} );
}

template <typename T, size_t N>
std::optional<std::array<T, N>> from_string( tag<std::array<T, N>>, std::string_view str ) noexcept
{
	std::array<T, N> out = {};
	tokenizer tok = { str };

	for ( size_t i = 0; i < N; ++i )
	{
		auto arg = tok.next();
		if ( !arg )
			return std::nullopt;

		if ( auto parsed_opt = from_string( tag<T>{}, *arg ); parsed_opt )
			out[i] = *parsed_opt;
		else
			return std::nullopt;
	}

	return out;
}

template <typename T> size_t to_chars( tag<std::span<T>>, std::span<char> buff, std::span<T> value ) noexcept
{
	if ( buff.size() < 3 ) // We need at least 2 chars for '{}' and 1 for null-terminator
		return 0;

	auto b = buff;
	for ( const auto &v : value )
	{
		size_t len = detail::to_chars_append( b, v, true );
		if ( len == 0 )
			return 0;
	}

	if ( b.size() < 2 ) // Not enough space for closing '}' and null-terminator
		return 0;

	buff[0] = '{'; // Replace leading ' ' with '{'
	b[0] = '}';
	b[1] = '\0';
	return b.data() - buff.data() + 2;
}

template <typename T, size_t N>
size_t to_chars( tag<std::array<T, N>>, std::span<char> buff, const std::array<T, N> &value ) noexcept
{
	return to_chars( tag<std::span<const T>>{}, buff, std::span<const T>{ value.data(), N } );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
  requires std::is_class_v<T> && detail::is_struct_bindable<T>
constexpr std::string_view type_name( tag<T> ) noexcept
{
	return "object";
}

template <typename T>
  requires std::is_class_v<T> && detail::is_struct_bindable<T>
std::optional<T> from_string( tag<T>, std::string_view str ) noexcept
{
	T obj{};
	conco::tokenizer tok{ str };

	bool still_valid = true;
	auto parse_member = [&]( auto &member ) {
		if ( !still_valid )
			return;

		if ( token arg = tok.next(); arg )
		{
			if ( auto parsed_opt = from_string( tag<std::remove_cvref_t<decltype( member )>>{}, *arg ); parsed_opt )
			{
				member = *parsed_opt;
				return;
			}
		}

		still_valid = false;
	};

	if constexpr ( detail::has_n_members_v<T, 9> )
	{
		static_assert( false, "Too many members in this class type!" );
	}
	else if constexpr ( detail::has_n_members_v<T, 8> )
	{
		auto &[m0, m1, m2, m3, m4, m5, m6, m7] = obj;
		auto ms_tuple = std::tie( m0, m1, m2, m3, m4, m5, m6, m7 );
		std::apply( [&]( auto &...ms ) { ( parse_member( ms ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 7> )
	{
		auto &[m0, m1, m2, m3, m4, m5, m6] = obj;
		auto ms_tuple = std::tie( m0, m1, m2, m3, m4, m5, m6 );
		std::apply( [&]( auto &...ms ) { ( parse_member( ms ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 6> )
	{
		auto &[m0, m1, m2, m3, m4, m5] = obj;
		auto ms_tuple = std::tie( m0, m1, m2, m3, m4, m5 );
		std::apply( [&]( auto &...ms ) { ( parse_member( ms ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 5> )
	{
		auto &[m0, m1, m2, m3, m4] = obj;
		auto ms_tuple = std::tie( m0, m1, m2, m3, m4 );
		std::apply( [&]( auto &...ms ) { ( parse_member( ms ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 4> )
	{
		auto &[m0, m1, m2, m3] = obj;
		auto ms_tuple = std::tie( m0, m1, m2, m3 );
		std::apply( [&]( auto &...ms ) { ( parse_member( ms ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 3> )
	{
		auto &[m0, m1, m2] = obj;
		auto ms_tuple = std::tie( m0, m1, m2 );
		std::apply( [&]( auto &...ms ) { ( parse_member( ms ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 2> )
	{
		auto &[m0, m1] = obj;
		auto ms_tuple = std::tie( m0, m1 );
		std::apply( [&]( auto &...ms ) { ( parse_member( ms ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 1> )
	{
		auto &[m0] = obj;
		auto ms_tuple = std::tie( m0 );
		std::apply( [&]( auto &...ms ) { ( parse_member( ms ), ... ); }, ms_tuple );
	}
	else
	{
		static_assert( false, "Class type has no ms!" );
	}

	if ( !still_valid )
		return std::nullopt;

	return obj;
}

template <typename T>
  requires std::is_class_v<T> && detail::is_struct_bindable<T>
size_t to_chars( tag<T>, std::span<char> buff, const T &value ) noexcept
{
	if ( buff.size() < 3 ) // We need at least 2 chars for '{}' and 1 for null-terminator
		return 0;

	auto b = buff;

	if constexpr ( detail::has_n_members_v<T, 9> )
	{
		static_assert( false, "Too many members in this class type!" );
	}
	else if constexpr ( detail::has_n_members_v<T, 8> )
	{
		const auto &[m0, m1, m2, m3, m4, m5, m6, m7] = value;
		auto ms_tuple = std::tie( m0, m1, m2, m3, m4, m5, m6, m7 );
		std::apply( [&]( const auto &...ms ) { ( detail::to_chars_append( b, ms, true ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 7> )
	{
		const auto &[m0, m1, m2, m3, m4, m5, m6] = value;
		auto ms_tuple = std::tie( m0, m1, m2, m3, m4, m5, m6 );
		std::apply( [&]( const auto &...ms ) { ( detail::to_chars_append( b, ms, true ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 6> )
	{
		const auto &[m0, m1, m2, m3, m4, m5] = value;
		auto ms_tuple = std::tie( m0, m1, m2, m3, m4, m5 );
		std::apply( [&]( const auto &...ms ) { ( detail::to_chars_append( b, ms, true ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 5> )
	{
		const auto &[m0, m1, m2, m3, m4] = value;
		auto ms_tuple = std::tie( m0, m1, m2, m3, m4 );
		std::apply( [&]( const auto &...ms ) { ( detail::to_chars_append( b, ms, true ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 4> )
	{
		const auto &[m0, m1, m2, m3] = value;
		auto ms_tuple = std::tie( m0, m1, m2, m3 );
		std::apply( [&]( const auto &...ms ) { ( detail::to_chars_append( b, ms, true ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 3> )
	{
		const auto &[m0, m1, m2] = value;
		auto ms_tuple = std::tie( m0, m1, m2 );
		std::apply( [&]( const auto &...ms ) { ( detail::to_chars_append( b, ms, true ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 2> )
	{
		const auto &[m0, m1] = value;
		auto ms_tuple = std::tie( m0, m1 );
		std::apply( [&]( const auto &...ms ) { ( detail::to_chars_append( b, ms, true ), ... ); }, ms_tuple );
	}
	else if constexpr ( detail::has_n_members_v<T, 1> )
	{
		const auto &[m0] = value;
		auto ms_tuple = std::tie( m0 );
		std::apply( [&]( const auto &...ms ) { ( detail::to_chars_append( b, ms, true ), ... ); }, ms_tuple );
	}
	else
	{
		static_assert( false, "Class type has no members!" );
	}

	if ( b.size() < 2 ) // Not enough space for closing '}' and null-terminator
		return 0;

	buff[0] = '{'; // Replace leading ' ' with '{'
	b[0] = '}';
	b[1] = '\0';
	return b.data() - buff.data() + 2;
}

} // namespace conco
