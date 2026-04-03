#ifndef BOOST_SPIRIT_X4_CORE_ACTION_HPP
#define BOOST_SPIRIT_X4_CORE_ACTION_HPP

/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman
    Copyright (c) 2025 Nana Sakisaka

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/spirit/config.hpp>
#include <boost/spirit/x4/core/parser.hpp>
#include <boost/spirit/x4/core/context.hpp>
#include <boost/spirit/x4/core/action_context.hpp>

#include <ranges>
#include <iterator>
#include <concepts>
#include <type_traits>
#include <utility>

namespace boost::spirit::x4 {

namespace detail {

struct raw_attribute_t;

template<class Context, X4Attribute Attr>
struct action_context;

template<class Context, X4NonUnusedAttribute Attr>
struct action_context<Context, Attr>
{
    using type = context<contexts::attr, Attr, canonical_context_t<Context const&>>;
};

template<class Context, X4UnusedAttribute Attr>
struct action_context<Context, Attr>
{
    using type = Context const&;
};

} // detail

// Note about the constraint on the `Action` parameter:
//
// Ideally we should have a context-agnostic concept that can be used
// like `X4ActionFunctor<F>`, but we technically can't.
//
// In order to check `std::invocable`, we need to know the actual context
// type passed to the `.parse(...)` function but it is unknown until
// runtime.
//
// Even if we make up the most trivial context type (i.e. `unused_type`),
// such concept will be useless because a user-provided functor always
// operates on user-specific precondition that assumes the context
// holds exact specific type provided to the entry point (`x4::parse`).

template<class Subject, class ActionF>
struct action : proxy_parser<Subject, action<Subject, ActionF>>
{
    static_assert(
        !std::is_reference_v<ActionF>,
        "Reference type is disallowed for semantic action functor to prevent dangling reference"
    );

    using base_type = proxy_parser<Subject, action>;

    static constexpr bool has_action = true;
    static constexpr bool need_rcontext = true;

    ActionF f;

    template<class SubjectT, class ActionT>
        requires std::is_constructible_v<base_type, SubjectT> && std::is_constructible_v<ActionF, ActionT>
    constexpr action(SubjectT&& subject, ActionT&& f)
        noexcept(std::is_nothrow_constructible_v<base_type, SubjectT> && std::is_nothrow_constructible_v<ActionF, ActionT>)
        : base_type(std::forward<SubjectT>(subject))
        , f(std::forward<ActionT>(f))
    {
    }

    // attr==unused, action wants attribute
    template<std::forward_iterator It, std::sentinel_for<It> Se, class Context>
    [[nodiscard]] constexpr bool
    parse(It& first, Se const& last, Context const& ctx, unused_type) const
        noexcept(
            std::is_nothrow_default_constructible_v<typename base_type::attribute_type> &&
            noexcept(this->parse_main(first, last, ctx, std::declval<typename base_type::attribute_type&>()))
        )
    {
        // Synthesize the attribute since one is not supplied
        typename base_type::attribute_type attribute; // default-initialize
        return this->parse_main(first, last, ctx, attribute);
    }

    // Catch-all overload for non-unused_type attribute
    template<std::forward_iterator It, std::sentinel_for<It> Se, class Context, X4Attribute Attr>
    [[nodiscard]] constexpr bool
    parse(It& first, Se const& last, Context const& ctx, Attr& attr) const
        noexcept(noexcept(this->parse_main(first, last, ctx, attr)))
    {
        return this->parse_main(first, last, ctx, attr);
    }

    constexpr void operator[](auto const&) const = delete; // You can't add semantic action for semantic action

private:
    // Semantic action with no parameter: `p[([] { /* ... */ })]`
    template<class Context, X4Attribute Attr>
    [[nodiscard]] constexpr bool
    call_action(Context const&, Attr&) const
        noexcept(std::is_nothrow_invocable_v<ActionF const&>)
    {
        // Explicitly make this hard error instead of emitting "no matching overload".
        // This provides much more human-friendly errors.
        static_assert(
            std::invocable<ActionF const&>,
            "Neither `f(ctx)` nor `f()` is well-formed for your semantic action. "
            "Check your function signature. Note that some functors might need "
            "`const` qualifier to satisfy the constraints."
        );

        using action_return_type = std::invoke_result_t<ActionF const&>;
        constexpr bool action_returns_bool = std::same_as<action_return_type, bool>;
        static_assert(
            action_returns_bool || std::same_as<action_return_type, void>,
            "Semantic action should not return value other than `bool`. Check your function signature."
        );

        if constexpr (action_returns_bool) {
            return this->f();
        } else {
            this->f();
            return true;
        }
    }

    // Semantic action with parameter: `p[([](auto&& ctx) { /* ... */ })]`
    template<class Context, X4Attribute Attr>
        requires std::invocable<ActionF const&, typename detail::action_context<Context, Attr>::type>
    [[nodiscard]] constexpr bool
    call_action(Context const& ctx, Attr& attr) const
        noexcept(std::is_nothrow_invocable_v<ActionF const&, typename detail::action_context<Context, Attr>::type>)
    {
        using action_return_type = std::invoke_result_t<ActionF const&, typename detail::action_context<Context, Attr>::type>;
        constexpr bool action_returns_bool = std::same_as<action_return_type, bool>;
        static_assert(
            action_returns_bool || std::same_as<action_return_type, void>,
            "Semantic action should not return value other than `bool`. Check your function signature."
        );

        // Inject `_attr` only when `Attr` is not `unused_type`
        if constexpr (X4UnusedAttribute<Attr>) {
            if constexpr (action_returns_bool) {
                return this->f(ctx);
            } else {
                this->f(ctx);
                return true;
            }

        } else {
            if constexpr (action_returns_bool) {
                return this->f(x4::make_context<contexts::attr>(attr, ctx));
            } else {
                this->f(x4::make_context<contexts::attr>(attr, ctx));
                return true;
            }
        }
    }

    template<class Context, X4Attribute Attr>
        requires
            (!std::invocable<ActionF const&, typename detail::action_context<Context, Attr>::type>) &&
            std::invocable<ActionF const&, typename detail::action_context<Context, Attr>::type const&>
    static constexpr bool
    call_action(Context const&, Attr&)
    {
        static_assert(
            std::invocable<ActionF const&, typename detail::action_context<Context, Attr>::type>,
            "Semantic action expecting non-const lvalue reference context is obsolete. Use `auto&& ctx` and avoid using `auto& ctx`."
        );
        return false; // dummy
    }

    template<std::forward_iterator It, std::sentinel_for<It> Se, class Context, X4Attribute Attr>
    [[nodiscard]] constexpr bool
    parse_main(It& first, Se const& last, Context const& ctx, Attr& attr) const
        noexcept(
            std::is_copy_assignable_v<It> &&
            is_nothrow_parsable_v<Subject, It, Se, Context, Attr> &&
            noexcept(this->call_action(ctx, attr))
        )
    {
        It const saved_first = first;
        if (!this->subject.parse(first, last, ctx, attr)) return false;

        if (this->call_action(ctx, attr)) {
            return true;
        }
        // reset iterators if semantic action failed the match
        // retrospectively
        first = saved_first;
        return false;
    }

    // attr==raw, action wants iterator_range (see raw.hpp)
    template<std::forward_iterator It, std::sentinel_for<It> Se, class Context>
    [[nodiscard]] constexpr bool
    parse_main(It& first, Se const& last, Context const& ctx, detail::raw_attribute_t&) const
        noexcept(false) // construction of `subrange` is never noexcept as per the standard
    {
        // synthesize the attribute since one is not supplied
        std::ranges::subrange<It, It> rng; // This must be It-It pair, NOT It-Se pair
        return this->parse_main(first, last, ctx, rng);
    }
};

template<X4Subject Subject, class Action>
[[nodiscard, deprecated(
    "Use `operator[]` instead. The symbol `/` normally means \"ordered choice\" "
    "in PEG, and is irrelevant to semantic actions. Furthermore, using C++'s "
    "`operator/` for this purpose may introduce surprising behavior when it's "
    "mixed with ordinary PEG operators, for instance, the unary `operator+`, "
    "due to precedence."
)]]
constexpr action<as_parser_plain_t<Subject>, std::remove_cvref_t<Action>>
operator/(Subject&& p, Action&& f)
    noexcept(
        is_parser_nothrow_castable_v<Subject> &&
        std::is_nothrow_constructible_v<
            action<as_parser_plain_t<Subject>, std::remove_cvref_t<Action>>,
            as_parser_t<Subject>, Action
        >
    )
{
    return {as_parser(std::forward<Subject>(p)), std::forward<Action>(f)};
}

} // boost::spirit::x4

#endif
