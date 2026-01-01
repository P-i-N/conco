#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>

#include "conco_tokenizer.hpp"

namespace conco {

enum class result
{
	success,                // Command executed successfully
	command_not_found,      // No command with the given name was found in provided list
	argument_parsing_error, // One or more arguments could not be parsed
	not_enough_arguments,   // Not enough arguments were provided for the command
	no_matching_overload,   // Multiple overloads found, but none could be executed due to argument parsing errors
};

/**
 * Executes a command from the given command list based on the provided command line string.
 */
result execute( std::span<const struct command> commands, std::string_view cmd_line, struct output &out );

/**
 * Same as above, but creates an `output` structure internally.
 */
result execute( std::span<const struct command> commands,
                std::string_view cmd_line,
                std::span<char> output_buffer = {} );

/**
 * Represents a single executable command.
 *
 * Commands can be created for free functions, lambdas or member functions (methods).
 * Everything is type-erased, runtime information about arguments, return type and
 * invocation are stored separately under `desc` member.
 *
 * The whole structure fits nicely into 24 bytes on 64-bit systems or 12 bytes on 32-bit systems.
 */
struct command final
{
	// Type-erased pointer to a function or an object instance (for method commands)
	void *target = nullptr;

	// Runtime info about arguments and invocation function
	const struct descriptor &desc;

	// Command name with optional arg. names ("sum x y"), followed by optional description block
	// separated by semicolon ("sum x y;Sum two integers")
	const char *name_and_args = nullptr;

	// Constructors for function commands. For method commands, use `method<>()` helper function.
	template <typename F>
	  requires std::is_function_v<std::remove_pointer_t<F>>
	command( F func, const char *n );

	// Constructors for callable objects/capturing lambda commands.
	template <typename C>
	  requires std::is_class_v<C>
	command( C &callable, const char *n );

	// Compares only the command name part, ignoring optional argument names and description
	bool operator==( std::string_view name ) const noexcept
	{
		size_t i = 0;
		while ( i < name.size() && name[i] && name[i] == name_and_args[i] )
			++i;

		return i == name.size() && tokenizer::is_ident_term( name_and_args[i] );
	}

private:
	template <typename C>
	command( const C &ctx, const descriptor &d, const char *n )
	  : target( const_cast<C *>( &ctx ) ), desc( d ), name_and_args( n )
	{}

	template <auto M, typename C> friend command method( C &ctx, const char *n );
	template <auto M, typename C> friend command method( const C &ctx, const char *n );
};

/**
 * Holds the result of a command execution with detailed error information.
 *
 * You only provide the buffer for storing stringified command result, the rest is filled
 * by the execution function and invokers.
 */
struct output
{
	std::span<char> buffer;            // Buffer for stringified command result
	const command *cmd = nullptr;      // Executed command, `nullptr` = not found
	uint32_t arg_error_mask = 0;       // Bitmask of argument parsing errors
	uint8_t arg_count = 0;             // Number of successfuly parsed arguments
	bool not_enough_arguments = false; // Whether there were not enough arguments
	bool result_error = false;         // Result stringification failed (does not mean execution failed!)

	bool has_error() const noexcept { return arg_error_mask || not_enough_arguments || result_error; }
};

/**
 * Encapsulates all the context needed for command execution.
 *
 * This structure is created by the `execute()` function and passed down to invokers and
 * argument parsers. You can also access it from your own command functions by using
 * `const context &` as one of your argument types.
 */
struct context final
{
	std::span<const command> commands; // All available commands (as provided to `execute()`)
	std::string_view raw_command_line; // Command line text buffer from the user
	std::string_view command_name;     // Command name (first token)
	tokenizer args;                    // Tokenizer for command arguments
	tokenizer default_args;            // Tokenizer for default arguments (if any)
	output &out;                       // Result of the command execution
};

/**
 * Holds runtime stringified type information about a single argument or return type.
 *
 * This is used in `descriptor` below to describe argument and return types of commands.
 * All names are filled from `type_name()` functions at compile-time.
 */
struct type_info
{
	std::string_view name = {};                       // "int", "optional", "vector", "my_struct", etc.
	const type_info *const inner_type_info = nullptr; // Pointer to inner type info, if any (usually the <T> type)

	// Gets the `type_info` instance for the given type `T`. `void` type results in `nullptr`.
	template <typename T> static constexpr const type_info *const get() noexcept;
};

/**
 * Holds runtime information about the command function or method.
 *
 * All the fields are filled from function/method signature traits at compile-time.
 * This is a "poor man's" reflection for command functions. You can use this to inspect
 * commands at runtime, generate help texts about arguments, etc.
 */
struct descriptor final
{
	using invoker_func_t = bool ( * )( struct context & );
	invoker_func_t invoker = nullptr;

	std::span<const type_info *const> arg_type_infos;
	const type_info *result_type_info = nullptr;

	uint8_t arg_count = 0;         // Number of real (program) arguments
	uint8_t command_arg_count = 0; // Number of textual arguments, minimum required
	bool has_tail_args = false;    // Whether the last argument is a tokenizer for variadic tail arguments

	template <typename I, typename Traits = typename I::traits> static const descriptor &get()
	{
		static const descriptor desc = { .invoker = &I::call,
			                               // Explicitly construct span with `arg_count` to trim the dummy element
			                               .arg_type_infos = { Traits::arg_type_infos, Traits::arg_count },
			                               .result_type_info = Traits::result_type_info,
			                               .arg_count = Traits::arg_count,
			                               .command_arg_count = Traits::command_arg_count,
			                               .has_tail_args = Traits::has_tail_args };

		return desc;
	}
};

template <typename T> struct tag
{
	using inner_type = void;
};

constexpr std::string_view type_name( auto ) noexcept
{
	return std::string_view(); // Empty fallback for unknown types
}

template <typename T> constexpr std::string_view type_name( tag<std::optional<T>> ) noexcept
{
	return "optional";
}

template <typename T> struct tag<std::optional<T>>
{
	using inner_type = T;
};

template <typename T> inline static constexpr const type_info *const type_info::get() noexcept
{
	static constexpr const type_info ti = { .name = type_name( tag<T>{} ),
		                                      .inner_type_info = []() noexcept -> const type_info * {
		                                        using inner_type = typename tag<T>::inner_type;

		                                        if constexpr ( std::is_same_v<inner_type, void> == false )
			                                        return type_info::get<typename tag<T>::inner_type>();
		                                        else
			                                        return nullptr;
		                                      }() };

	return &ti;
}

} // namespace conco

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Here be dragons... */

namespace conco::detail {

struct any
{
	template <typename T> operator T() const;
};

template <typename T, std::size_t N>
concept has_n_members = []<std::size_t... Is>( std::index_sequence<Is...> ) {
	return requires { std::remove_cvref_t<T>{ ( void( Is ), detail::any{} )... }; };
}( std::make_index_sequence<N>{} );

template <typename T, std::size_t N> struct has_n_members_t : std::bool_constant<has_n_members<T, N>>
{};

template <typename... Args, std::size_t N>
struct has_n_members_t<std::tuple<Args...>, N> : std::bool_constant<sizeof...( Args ) == N>
{};

template <typename T, std::size_t N> constexpr bool has_n_members_v = has_n_members_t<T, N>::value;

template <typename T> struct arg_type_helper
{
	using arg_tuple_type = std::remove_cvref_t<T>; // How the parsed argument will appear in the args tuple
	static constexpr size_t command_arg_count = 1; // Number of command arguments this type consumes
	template <typename T> static T &to_call_arg( T &value ) noexcept { return value; }
};

template <typename U>
  requires std::is_same_v<std::remove_cvref_t<U>, tokenizer>
struct arg_type_helper<U>
{
	using arg_tuple_type = U;                      // As-is
	static constexpr size_t command_arg_count = 0; // Does not consume any command arguments
	static U &to_call_arg( U &value ) noexcept { return value; }
};

template <> struct arg_type_helper<output &>
{
	using arg_tuple_type = output &;               // As-is
	static constexpr size_t command_arg_count = 0; // Does not consume any command arguments
	static output &to_call_arg( output &value ) noexcept { return value; }
};

template <> struct arg_type_helper<const context &>
{
	using arg_tuple_type = const context &;        // As-is
	static constexpr size_t command_arg_count = 0; // Does not consume any command arguments
	static const context &to_call_arg( const context &value ) noexcept { return value; }
};

template <typename T> struct std_optional_helper : std::false_type
{};

template <typename U> struct std_optional_helper<std::optional<U>> : std::true_type
{
	using type = U;
};

// Extracts the signature from member function pointer types
template <typename T> struct signature_helper;

template <typename C, typename RT, typename... Args> struct signature_helper<RT ( C::* )( Args... )>
{
	using type = RT( Args... );
	static constexpr bool is_const = false;
};

template <typename C, typename RT, typename... Args>
struct signature_helper<RT ( C::* )( Args... ) noexcept> : signature_helper<RT ( C::* )( Args... )>
{};

template <typename C, typename RT, typename... Args> struct signature_helper<RT ( C::* )( Args... ) const>
{
	using type = RT( Args... );
	static constexpr bool is_const = true;
};

template <typename C, typename RT, typename... Args>
struct signature_helper<RT ( C::* )( Args... ) const noexcept> : signature_helper<RT ( C::* )( Args... ) const>
{};

template <typename F> struct command_traits;

// Reflects function/method signatures into information about arguments and return type
template <typename RT, typename... Args> struct command_traits<RT( Args... )>
{
	static_assert( sizeof...( Args ) <= ( sizeof( output::arg_count ) * 8 ), "Too many command arguments!" );
	static_assert( ( !std::is_rvalue_reference_v<Args> && ... ), "Command arguments cannot be r-value references (&&)!" );

	using return_type = RT;

	static constexpr size_t arg_count = sizeof...( Args );
	static constexpr size_t command_arg_count = ( ( arg_type_helper<Args>::command_arg_count ) + ... + 0 );

	static constexpr bool has_tail_args = ( std::is_same_v<std::remove_cvref_t<Args>, tokenizer> || ... || false );

	static constexpr const type_info *const arg_type_infos[sizeof...( Args ) + 1] = {
		type_info::get<std::remove_cvref_t<Args>>()..., {} // Dummy +1 so the array is not empty
	};

	static constexpr const type_info *const result_type_info = type_info::get<RT>();

	static_assert( command_arg_count <= arg_count,
	               "Number of command arguments cannot be greater than total number of arguments!" );
};

/**
 * Extracts the next argument value from the command line or try to take one from default arguments.
 */
inline token next_arg_value( context &ctx ) noexcept
{
	token default_value = std::nullopt;
	if ( ctx.default_args.next() ) // Consume optional argument name
	{
		if ( ctx.default_args.consume_char_if( '=' ) )
			default_value = ctx.default_args.next();
	}

	token arg = ctx.args.next();
	if ( !arg && default_value )
		arg = default_value;

	return arg;
}

/**
 * For generic types, extracts next argument from the command line and attempts to parse it
 * into the desired type. Some special "built-in" types are returned directly without parsing.
 */
template <typename T> static T parse_arg( context &ctx ) noexcept
{
	// `tokenizer` allowed as whatever, it will only see remaining tail arguments
	if constexpr ( std::is_same_v<std::remove_cvref_t<T>, tokenizer> )
		return ctx.args;
	// `output` only allowed as ref, because user *IS* expected to modify it
	else if constexpr ( std::is_same_v<T, output &> )
		return ctx.out;
	// `context` only allowed as const ref, so the user can't modify it from inside command functions
	else if constexpr ( std::is_same_v<T, const context &> )
		return ctx;
	// std::optional<U>
	else if constexpr ( std_optional_helper<T>::value )
	{
		if ( token arg = next_arg_value( ctx ); arg )
		{
			++ctx.out.arg_count;

			using U = typename std_optional_helper<T>::type;
			if ( auto parsed_opt = from_string( tag<U>{}, *arg ); parsed_opt )
				return *parsed_opt;

			ctx.out.arg_error_mask |= static_cast<uint32_t>( 1u << ( ctx.out.arg_count - 1 ) );
		}

		return std::nullopt;
	}
	// Everything else is parsed and converted from string into the desired type
	else
	{
		if ( token arg = next_arg_value( ctx ); arg )
		{
			++ctx.out.arg_count;

			if ( auto parsed_opt = from_string( tag<T>{}, *arg ); parsed_opt )
				return *parsed_opt;

			ctx.out.arg_error_mask |= static_cast<uint32_t>( 1u << ( ctx.out.arg_count - 1 ) );
		}
		else
		{
			ctx.out.not_enough_arguments = true;
		}

		return T{};
	}
}

/**
 * Creates a tuple of parsed arguments for the given command function/method.
 *
 * For example, for a function `void foo(int, float, std::string)`, this will return
 * a tuple: `std::tuple<int, float, std::string>` with all arguments parsed from the
 * command context.
 */
template <typename... Args> auto make_args_tuple( context &ctx ) noexcept
{
	using result_t = std::tuple<typename arg_type_helper<Args>::arg_tuple_type...>;

	// Using brace initialization to guarantee left-to-right evaluation order or `parse_arg<>()` calls
	return result_t{ ( parse_arg<typename arg_type_helper<Args>::arg_tuple_type>( ctx ) )... };
}

// Applies given tuple of arguments to the callable (function/method) and handles result stringification
template <typename RT, typename... Args> static bool apply( context &ctx, auto &&callable, auto &&args_tuple )
{
	if constexpr ( std::is_void_v<RT> )
	{
		ctx.out.result_error = false;

		auto cb = [&callable]( auto &&...args ) { callable( arg_type_helper<Args>::to_call_arg( args )... ); };
		std::apply( cb, args_tuple );
		if ( !ctx.out.buffer.empty() )
			ctx.out.buffer[0] = '\0';
	}
	else
	{
		auto cb = [&callable]( auto &&...args ) -> RT { return callable( arg_type_helper<Args>::to_call_arg( args )... ); };
		auto r = std::apply( cb, args_tuple );

		if constexpr ( std_optional_helper<RT>::value )
		{
			using U = typename std_optional_helper<RT>::type;

			if ( !ctx.out.buffer.empty() )
			{
				if ( r )
					ctx.out.result_error = ( to_chars( tag<U>{}, ctx.out.buffer, *r ) == 0 );
				else
					ctx.out.buffer[0] = '\0';
			}
		}
		else
		{
			if ( !ctx.out.buffer.empty() )
				ctx.out.result_error = ( to_chars( tag<RT>{}, ctx.out.buffer, r ) == 0 );
		}
	}

	return true;
}

template <typename F> struct function_invoker;

template <typename RT, typename... Args> struct function_invoker<RT ( * )( Args... )> : command_traits<RT( Args... )>
{
	using traits = command_traits<RT( Args... )>;

	static bool call( context &ctx )
	{
		auto args_tuple = make_args_tuple<Args...>( ctx );
		if ( ctx.out.has_error() )
			return false;

		auto callable = static_cast<RT ( * )( Args... )>( ctx.out.cmd->target );
		return apply<RT, Args...>( ctx, callable, args_tuple );
	}
};

template <typename C, typename F> struct callable_invoker_impl;

template <typename C, typename RT, typename... Args>
struct callable_invoker_impl<C, RT( Args... )> : command_traits<RT( Args... )>
{
	using traits = command_traits<RT( Args... )>;

	static bool call( context &ctx )
	{
		auto args_tuple = make_args_tuple<Args...>( ctx );
		if ( ctx.out.has_error() )
			return false;

		auto *obj = static_cast<C *>( ctx.out.cmd->target );
		auto callable = [obj]( auto &&...args ) -> RT { return ( *obj )( std::forward<decltype( args )>( args )... ); };
		return apply<RT, Args...>( ctx, callable, args_tuple );
	}
};

template <typename C>
struct callable_invoker : callable_invoker_impl<C, typename signature_helper<decltype( &C::operator() )>::type>
{};

template <typename C, auto M, typename F> struct method_invoker_impl;

template <typename C, auto M, typename RT, typename... Args>
struct method_invoker_impl<C, M, RT( Args... )> : command_traits<RT( Args... )>
{
	using traits = command_traits<RT( Args... )>;

	static bool call( context &ctx )
	{
		auto args_tuple = make_args_tuple<Args...>( ctx );
		if ( ctx.out.has_error() )
			return false;

		auto *obj = static_cast<C *>( ctx.out.cmd->target );
		auto callable = [obj]( auto &&...args ) -> RT { return ( obj->*M )( std::forward<decltype( args )>( args )... ); };
		return apply<RT, Args...>( ctx, callable, args_tuple );
	}
};

template <typename C, auto M>
struct method_invoker : method_invoker_impl<C, M, typename signature_helper<decltype( M )>::type>
{
	using method_traits = signature_helper<decltype( M )>;
	static_assert( !std::is_const_v<C> || method_traits::is_const,
	               "Non-const method invoker instantiated for const instance!" );
};

} // namespace conco::detail

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace conco {

template <typename F>
  requires std::is_function_v<std::remove_pointer_t<F>>
inline command::command( F func, const char *n )
  : target( ( void * )func ), desc( descriptor::get<detail::function_invoker<F>>() ), name_and_args( n )
{}

template <typename C>
  requires std::is_class_v<C>
inline command::command( C &callable, const char *n )
  : target( &callable ), desc( descriptor::get<detail::callable_invoker<C>>() ), name_and_args( n )
{}

template <auto M, typename C> command method( C &ctx, const char *n )
{
	return { ctx, descriptor::get<detail::method_invoker<C, M>>(), n };
}

template <auto M, typename C> command method( const C &ctx, const char *n )
{
	return { ctx, descriptor::get<detail::method_invoker<const C, M>>(), n };
}

inline result execute( std::span<const command> commands, std::string_view cmd_line, output &out )
{
	size_t overload_count = 0;

	tokenizer tok( cmd_line );
	std::string_view command_name = tok.next().value_or( std::string_view() );

	auto cmd_iter = std::ranges::find_if( commands, [&]( const command &cmd ) { return cmd == command_name; } );
	if ( cmd_iter == commands.end() )
		return result::command_not_found;

	while ( true )
	{
		++overload_count;

		tokenizer default_args( cmd_iter->name_and_args + command_name.size() );
		context ctx = { commands, cmd_line, command_name, tok, default_args, out };

		out = { out.buffer, &*cmd_iter };

		if ( out.cmd->desc.invoker( ctx ) )
			return result::success;

		++cmd_iter;

		if ( cmd_iter == commands.end() || *cmd_iter != command_name )
			break;
	}

	if ( overload_count == 1 )
		return out.not_enough_arguments ? result::not_enough_arguments : result::argument_parsing_error;

	return overload_count > 1 ? result::no_matching_overload : result::command_not_found;
}

inline result execute( std::span<const command> commands, std::string_view cmd_line, std::span<char> output_buffer )
{
	output out = { output_buffer };
	return execute( commands, cmd_line, out );
}

} // namespace conco

#include "conco_types.hpp"
