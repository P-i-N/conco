#pragma once

#include <algorithm>
#include <cstdint>
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

	template <auto M, typename C>
	friend command method( C &ctx, const char *n );
	template <auto M, typename C>
	friend command method( const C &ctx, const char *n );
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

	token next_arg_value() noexcept
	{
		token default_value = std::nullopt;
		if ( default_args.next() ) // Consume optional argument name
		{
			if ( default_args.consume_char_if( '=' ) )
				default_value = default_args.next();
		}

		token arg = args.next();
		if ( !arg && default_value )
			arg = default_value;

		return arg;
	}
};

/**
 * Holds runtime stringified type information about a single argument or return type.
 * You can use this to provide better help texts, error messages, etc. at runtime.
 *
 * This is used in `descriptor` below to describe argument and return types of commands.
 * All names are filled from `type_name()` functions at compile-time.
 */
struct type_info
{
	std::string_view name = {};                       // "int", "optional", "vector", "my_struct", etc.
	const type_info *const inner_type_info = nullptr; // Pointer to inner type info, if any (usually the <T> type)

	// Gets the `type_info` instance for the given type `T`. `void` type results in `nullptr`.
	template <typename T>
	static constexpr const type_info *const get() noexcept;
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

	uint8_t arg_count = 0;      // Number of real (program) arguments
	bool has_tail_args = false; // Whether the last argument is a tokenizer for variadic tail arguments

	template <typename I, typename Traits = typename I::traits>
	static const descriptor &get()
	{
		static const descriptor desc = { .invoker = &I::call,
			                               // Explicitly construct span with `arg_count` to trim the dummy element
			                               .arg_type_infos = { Traits::arg_type_infos, Traits::arg_count },
			                               .result_type_info = Traits::result_type_info,
			                               .arg_count = Traits::arg_count,
			                               .has_tail_args = Traits::has_tail_args };

		return desc;
	}
};

/**
 * Empty tag type for type-based dispatching and easier specialization.
 */
template <typename T>
struct tag
{};

/**
 * Maps command function argument types into storage types used during parsing and invocation.
 *
 * For generic types, the storage type is the same as the argument type. Meaning - if your command
 * function takes `int`, the argument is intermediately stored as `int` as well.
 *
 * For more complex types, you can specialize this structure to provide different storage types.
 * Typical example: `const char *` function arguments require `std::string` storage to hold the
 * parsed string value.
 */
template <typename T>
struct type_mapper
{
	/**
	 * The inner type used for nested type information (e.g., `T` in `std::optional<T>`)
	 * This is used by `type_info` to build type hierarchies for complex types.
	 */
	using inner_type = void;

	/**
	 * The storage type used during argument parsing and invocation.
	 * This is where the parsed argument "lives" before being forwarded into the command function.
	 */
	using storage_type = std::remove_cvref_t<T>;

	/**
	 * Converts the stored argument value into the type expected by the command function.
	 * For generic types, this is just an identity function. Special types can do more complex
	 * conversions at this point (e.g., forward `std::vector<T>` storage into `std::span<T>`).
	 */
	static T map( storage_type &value ) noexcept { return static_cast<T>( value ); }
};

// Fallback specialization for `void` type - no storage, no forwarding
template <>
struct type_mapper<void>
{
	using inner_type = void;
};

template <typename T>
struct ref_type_mapper
{
	using inner_type = void;
	using storage_type = T &;
	static T &map( storage_type &value ) noexcept { return static_cast<T &>( value ); }
};

/**
 * Provides a human-friendly name for a specified type.
 * This is used to fill `type_info` structures at compile-time.
 */
constexpr std::string_view type_name( auto ) noexcept
{
	return std::string_view(); // Empty fallback for unknown types
}

template <typename T>
inline static constexpr const type_info *const type_info::get() noexcept
{
	static constexpr const type_info ti = { .name = type_name( tag<T>{} ),
		                                      .inner_type_info = []() noexcept -> const type_info * {
		                                        using inner_type = typename type_mapper<T>::inner_type;

		                                        if constexpr ( std::is_same_v<inner_type, void> == false )
			                                        return type_info::get<typename type_mapper<T>::inner_type>();
		                                        else
			                                        return nullptr;
		                                      }() };

	return &ti;
}

template <typename T, typename S = typename type_mapper<T>::storage_type>
static S parse( tag<T>, context &ctx ) noexcept
{
	if ( token arg = ctx.next_arg_value(); arg )
	{
		++ctx.out.arg_count;

		if ( auto parsed_opt = from_string( tag<S>{}, *arg ); parsed_opt )
			return *parsed_opt;

		ctx.out.arg_error_mask |= static_cast<uint32_t>( 1u << ( ctx.out.arg_count - 1 ) );
	}
	else
	{
		ctx.out.not_enough_arguments = true;
	}

	return S{};
}

static tokenizer &parse( tag<tokenizer>, context &ctx ) noexcept { return ctx.args; }

template <>
struct type_mapper<tokenizer> : ref_type_mapper<tokenizer &>
{};

static output &parse( tag<output>, context &ctx ) noexcept { return ctx.out; }

template <>
struct type_mapper<output> : ref_type_mapper<output &>
{};

static const context &parse( tag<context>, context &ctx ) noexcept { return ctx; }

template <>
struct type_mapper<context> : ref_type_mapper<const context &>
{};

} // namespace conco

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Here be dragons... */

namespace conco::detail {

// Extracts the signature from member function pointer types
template <typename T>
struct signature_helper;

template <typename C, typename RT, typename... Args>
struct signature_helper<RT ( C::* )( Args... )>
{
	using type = RT( Args... );
	static constexpr bool is_const = false;
};

template <typename C, typename RT, typename... Args>
struct signature_helper<RT ( C::* )( Args... ) noexcept> : signature_helper<RT ( C::* )( Args... )>
{};

template <typename C, typename RT, typename... Args>
struct signature_helper<RT ( C::* )( Args... ) const>
{
	using type = RT( Args... );
	static constexpr bool is_const = true;
};

template <typename C, typename RT, typename... Args>
struct signature_helper<RT ( C::* )( Args... ) const noexcept> : signature_helper<RT ( C::* )( Args... ) const>
{};

template <typename F>
struct command_traits;

// Reflects function/method signatures into information about arguments and return type
template <typename RT, typename... Args>
struct command_traits<RT( Args... )>
{
	static_assert( sizeof...( Args ) <= ( sizeof( output::arg_count ) * 8 ), "Too many command arguments!" );
	static_assert( ( !std::is_rvalue_reference_v<Args> && ... ), "Command arguments cannot be r-value references (&&)!" );

	using return_type = RT;

	static constexpr size_t arg_count = sizeof...( Args );

	static constexpr bool has_tail_args = ( std::is_same_v<std::remove_cvref_t<Args>, tokenizer> || ... || false );

	static constexpr const type_info *const arg_type_infos[sizeof...( Args ) + 1] = {
		type_info::get<std::remove_cvref_t<Args>>()..., {} // Dummy +1 so the array is not empty
	};

	static constexpr const type_info *const result_type_info = type_info::get<RT>();
};

/**
 * Creates a tuple of parsed arguments for the given command function/method.
 *
 * For example, for a function `void foo(int, float, std::string)`, this will return
 * a tuple: `std::tuple<int, float, std::string>` with all arguments parsed from the
 * command context.
 */
template <typename... Args>
auto make_args_tuple( context &ctx ) noexcept
{
	using result_t = std::tuple<typename type_mapper<std::remove_cvref_t<Args>>::storage_type...>;

	// Using brace initialization to guarantee left-to-right evaluation order or `parse()` calls
	return result_t{ ( parse( tag<std::remove_cvref_t<Args>>{}, ctx ) )... };
}

// Applies given tuple of arguments to the callable (function/method) and handles result stringification
template <typename RT>
static void apply( context &ctx, auto &callable, auto &args_tuple )
{
	ctx.out.result_error = false;

	if constexpr ( std::is_void_v<RT> )
	{
		std::apply( callable, args_tuple );
		if ( !ctx.out.buffer.empty() )
			ctx.out.buffer[0] = '\0';
	}
	else
	{
		auto r = std::apply( callable, args_tuple );
		if ( !ctx.out.buffer.empty() )
			ctx.out.result_error = ( to_chars( tag<RT>{}, ctx.out.buffer, r ) == 0 );
	}
}

template <typename F>
struct function_invoker;

template <typename RT, typename... Args>
struct function_invoker<RT ( * )( Args... )> : command_traits<RT( Args... )>
{
	using traits = command_traits<RT( Args... )>;

	static bool call( context &ctx )
	{
		auto args_tuple = make_args_tuple<Args...>( ctx );
		if ( ctx.out.has_error() )
			return false;

		auto *target = static_cast<RT ( * )( Args... )>( ctx.out.cmd->target );

		if constexpr ( std::is_void_v<RT> )
		{
			auto callable = [target]( auto &&...args ) {
				target( type_mapper<Args>::map( std::forward<decltype( args )>( args ) )... );
			};
			apply<RT>( ctx, callable, args_tuple );
		}
		else
		{
			auto callable = [target]( auto &&...args ) -> RT {
				return target( type_mapper<std::remove_cvref_t<Args>>::map( std::forward<decltype( args )>( args ) )... );
			};
			apply<RT>( ctx, callable, args_tuple );
		}

		return true;
	}
};

template <typename C, typename F>
struct callable_invoker_impl;

template <typename C, typename RT, typename... Args>
struct callable_invoker_impl<C, RT( Args... )> : command_traits<RT( Args... )>
{
	using traits = command_traits<RT( Args... )>;

	static bool call( context &ctx )
	{
		auto args_tuple = make_args_tuple<Args...>( ctx );
		if ( ctx.out.has_error() )
			return false;

		auto *target = static_cast<C *>( ctx.out.cmd->target );

		if constexpr ( std::is_void_v<RT> )
		{
			auto callable = [target]( auto &&...args ) {
				( *target )( type_mapper<Args>::map( std::forward<decltype( args )>( args ) )... );
			};
			apply<RT>( ctx, callable, args_tuple );
		}
		else
		{
			auto callable = [target]( auto &&...args ) -> RT {
				return ( *target )( type_mapper<Args>::map( std::forward<decltype( args )>( args ) )... );
			};
			apply<RT>( ctx, callable, args_tuple );
		}

		return true;
	}
};

template <typename C>
struct callable_invoker : callable_invoker_impl<C, typename signature_helper<decltype( &C::operator() )>::type>
{};

template <typename C, auto M, typename F>
struct method_invoker_impl;

template <typename C, auto M, typename RT, typename... Args>
struct method_invoker_impl<C, M, RT( Args... )> : command_traits<RT( Args... )>
{
	using traits = command_traits<RT( Args... )>;

	static bool call( context &ctx )
	{
		auto args_tuple = make_args_tuple<Args...>( ctx );
		if ( ctx.out.has_error() )
			return false;

		auto *target = static_cast<C *>( ctx.out.cmd->target );

		if constexpr ( std::is_void_v<RT> )
		{
			auto callable = [target]( auto &&...args ) {
				( target->*M )( type_mapper<Args>::map( std::forward<decltype( args )>( args ) )... );
			};
			apply<RT>( ctx, callable, args_tuple );
		}
		else
		{
			auto callable = [target]( auto &&...args ) -> RT {
				return ( target->*M )( type_mapper<Args>::map( std::forward<decltype( args )>( args ) )... );
			};
			apply<RT>( ctx, callable, args_tuple );
		}

		return true;
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

template <auto M, typename C>
command method( C &ctx, const char *n )
{
	return { ctx, descriptor::get<detail::method_invoker<C, M>>(), n };
}

template <auto M, typename C>
command method( const C &ctx, const char *n )
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
