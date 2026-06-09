/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */
#ifndef ESS_CONTAINER_AND_VIEW_AND_RANGE_HXX
#define ESS_CONTAINER_AND_VIEW_AND_RANGE_HXX
#include <concepts>
#include <exception>
#include <ranges>

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

}

#endif // ESS_CONTAINER_AND_VIEW_HXX
