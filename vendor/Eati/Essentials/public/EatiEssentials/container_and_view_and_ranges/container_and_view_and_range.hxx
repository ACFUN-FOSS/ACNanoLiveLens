/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */
#ifndef ESS_CONTAINER_AND_VIEW_AND_RANGE_HXX
#define ESS_CONTAINER_AND_VIEW_AND_RANGE_HXX
#include <concepts>
#include <exception>
#include <ranges>
#include <optional>
#include <memory>
#include <utility>
#include <type_traits>
#include <concepts>

namespace Essentials::ContainerAndView
{

template <std::ranges::input_range Rng, class Pred, class ExFactory>
requires std::predicate<
    Pred&, 
    const std::ranges::range_reference_t<Rng>
> && 
std::invocable<ExFactory&> &&
std::is_base_of_v<
    std::exception,
    std::remove_cvref_t<std::invoke_result_t<ExFactory&>>
>
auto findOrThrow(Rng&& range, Pred&& pred, ExFactory&& exFactory)
    -> std::ranges::range_reference_t<Rng>
{
    auto it = std::ranges::find_if(
    	std::forward<Rng>(range),
    	std::forward<Pred>(pred)
	);
    if (it == std::ranges::end(range))
        throw std::invoke(std::forward<ExFactory>(exFactory));

    return *it;
}


template<class ExFactory>
struct OrElseThrowAdaptor
{
    ExFactory exFactory;

    // optional<T>
    template<class T>
    decltype(auto) operator()(std::optional<T>& opt) const {
        if (!opt)
            throw exFactory();
        return opt.value();
    }

    template<class T>
    decltype(auto) operator()(std::optional<T>&& opt) const {
        if (!opt)
            throw exFactory();
        return std::move(opt.value());
    }

    // raw pointer
    template<class T>
    T& operator()(T* ptr) const {
        if (!ptr)
            throw exFactory();
        return *ptr;
    }

    // unique_ptr
    template<class T>
    T& operator()(std::unique_ptr<T>& ptr) const {
        if (!ptr)
            throw exFactory();
        return *ptr;
    }

    // shared_ptr
    template<class T>
    T& operator()(std::shared_ptr<T>& ptr) const {
        if (!ptr)
            throw exFactory();
        return *ptr;
    }
};

template<class ExFactory>
auto orElseThrow(ExFactory&& exFactory) {
    return OrElseThrowAdaptor<std::decay_t<ExFactory>>{
        std::forward<ExFactory>(exFactory)
    };
}

template<class L, class R>
decltype(auto) operator|(L&& lhs, R&& rhs) {
    return rhs(std::forward<L>(lhs));
}

}

#endif // ESS_CONTAINER_AND_VIEW_AND_RANGE_HXX
