#pragma once

#include "../conco.hpp"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace conco::detail {

// Concept for map/unordered_map-like types
template <typename M>
concept is_map_like = requires {
	typename M::key_type;
	typename M::mapped_type;
	typename M::value_type;
	{ M{}.emplace( std::declval<typename M::key_type>(), std::declval<typename M::mapped_type>() ) };
} && std::is_same_v<typename M::value_type, std::pair<const typename M::key_type, typename M::mapped_type>>;

template <typename T>
using inner_type = typename type_mapper<std::remove_cvref_t<T>>::inner_type;

template <typename T>
using storage_type = typename type_mapper<std::remove_cvref_t<T>>::storage_type;

} // namespace conco::detail

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace conco {

constexpr std::string_view type_name( tag<std::string> ) noexcept { return "string"; }

std::optional<std::string> from_string( tag<std::string>, std::string_view str ) noexcept { return std::string{ str }; }

size_t to_chars( tag<std::string>, std::span<char> buff, const std::string &value ) noexcept
{
	return to_chars( tag<std::string_view>{}, buff, std::string_view{ value } );
}

template <>
struct type_mapper<const char *>
{
	using inner_type = void;
	using storage_type = std::string;
	static const char *map( storage_type &value ) noexcept { return value.c_str(); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename A>
struct type_mapper<std::vector<T, A>>
{
	using inner_type = T;
	using storage_type = std::vector<typename type_mapper<T>::storage_type, A>;

	static storage_type &map( storage_type &value ) noexcept
	  requires std::is_same_v<typename type_mapper<T>::storage_type, T>
	{
		return value;
	}

	static std::vector<T, A> map( storage_type &value ) noexcept
	  requires !std::is_same_v<typename type_mapper<T>::storage_type, T>
	{
		std::vector<T, A> out;
		out.reserve( value.size() );

		for ( auto &v : value )
			out.push_back( type_mapper<T>::map( v ) );

		return out;
	}
};

template <typename T, typename A>
constexpr std::string_view type_name( tag<std::vector<T, A>> ) noexcept
{
	return "vector";
}

template <typename T>
struct type_mapper<std::span<T>>
{
	using inner_type = T;
	using storage_type = std::vector<typename type_mapper<T>::storage_type>;

	static std::span<T> map( storage_type &value ) noexcept
	  requires std::is_same_v<typename type_mapper<T>::storage_type, T>
	{
		return std::span<T>{ value.data(), value.size() };
	}

	template <typename U>
	struct span_wrapper
	{
		std::vector<U> vec;
		operator std::span<U>() noexcept { return std::span<U>{ vec.data(), vec.size() }; }
	};

	static span_wrapper<T> map( storage_type &value ) noexcept
	  requires !std::is_same_v<typename type_mapper<T>::storage_type, T>
	{
		span_wrapper<T> out;
		out.vec.reserve( value.size() );

		for ( auto &v : value )
			out.vec.push_back( type_mapper<T>::map( v ) );

		return out;
	}
};

template <typename T, typename A>
std::optional<std::vector<T, A>> from_string( tag<std::vector<T, A>>, std::string_view str ) noexcept
{
	std::vector<T, A> out;
	tokenizer tok = { str };

	while ( true )
	{
		auto arg = tok.next();
		if ( !arg )
			break;

		if ( auto parsed_opt = from_string( tag<T>{}, *arg ); parsed_opt )
			out.push_back( *parsed_opt );
		else
			return std::nullopt;
	}

	return out;
}

template <typename T, typename A>
size_t to_chars( tag<std::vector<T, A>>, std::span<char> buff, const std::vector<T, A> &value ) noexcept
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <detail::is_map_like M>
struct type_mapper<M>
{
	using key_type = typename M::key_type;
	using inner_type = typename M::mapped_type;
	using stored_inner_type = typename type_mapper<inner_type>::storage_type;

	using storage_type = std::map<key_type, stored_inner_type>;

	static storage_type &map( storage_type &value ) noexcept
	  requires std::is_same_v<stored_inner_type, inner_type>
	{
		return value;
	}

	static M map( storage_type &value ) noexcept
	  requires !std::is_same_v<stored_inner_type, inner_type>
	{
		M out;

		for ( auto &[k, v] : value )
			out.emplace( k, type_mapper<inner_type>::map( v ) );

		return out;
	}
};

template <typename K, typename T, typename P, typename A>
constexpr std::string_view type_name( tag<std::map<K, T, P, A>> ) noexcept
{
	return "map";
}

template <detail::is_map_like M>
std::optional<detail::storage_type<M>> from_string( tag<M>, std::string_view str ) noexcept
{
	detail::storage_type<M> out;

	using map_inner_type = type_mapper<detail::inner_type<M>>::storage_type;

	tokenizer tok = { str };
	while ( true )
	{
		auto key = tok.next();
		if ( !key )
			break;

		if ( !tok.consume_char_if( '=' ) )
			return std::nullopt;

		auto value = tok.next();
		if ( !value )
			return std::nullopt;

		auto parsed_key_opt = from_string( tag<typename M::key_type>{}, *key );
		auto parsed_value_opt = from_string( tag<map_inner_type>{}, *value );

		if ( parsed_key_opt && parsed_value_opt )
			out.emplace( *parsed_key_opt, *parsed_value_opt );
		else
			return std::nullopt;
	}

	return out;
}

template <detail::is_map_like M>
size_t to_chars( tag<M>, std::span<char> buff, const M &value ) noexcept
{
	if ( buff.size() < 3 ) // We need at least 2 chars for '{}' and 1 for null-terminator
		return 0;

	auto b = buff;
	for ( const auto &[key, val] : value )
	{
		size_t len = detail::to_chars_append( b, key, true );
		if ( len == 0 )
			return 0;

		if ( b.size() < 2 ) // Need at least '=' and something for value
			return 0;

		b[0] = '=';
		b = b.subspan( 1 );

		len = detail::to_chars_append( b, val, false );
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

} // namespace conco
