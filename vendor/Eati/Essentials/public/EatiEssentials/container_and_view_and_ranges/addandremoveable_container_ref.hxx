/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */
#ifndef ESS_ADDANDREMOVEABLE_REF_HXX
#define ESS_ADDANDREMOVEABLE_REF_HXX
#include <utility>
#include <iterator>
#include <gsl/pointers>
#include "EatiEssentials/memsafety.hxx"

namespace Essentials::ContainerAndView
{

template <typename C>
concept AddandremoveableContainer = requires(C &c) {
    typename C::value_type;
    c.emplace_back(std::declval<typename C::value_type>());
    { c.begin() } -> std::forward_iterator;
    { c.end() } -> std::forward_iterator;
    c.erase(c.begin());
};

template <typename T>
class IAddandremoveableContainerRef {
public:
    virtual ~IAddandremoveableContainerRef() = default;
    virtual void add(T &&value) = 0;
    virtual void remove(const T &value) = 0;
};

/**
 * @class AddandremoveableContainerRef
 * @brief A reference class for containers that supports adding and removing elements.
 *
 * This class wraps around a container, providing an interface to add or remove elements
 * from the underlying container. It is designed to work with containers that meet the
 * `AddandremoveableContainer` concept, which requires the container to support emplacement,
 * iteration, and erasure of elements.
 *
 * @tparam T The type of elements stored in the container.
 * @tparam Container The type of the container being wrapped.
 */
template <typename T, typename Container>
    requires AddandremoveableContainer<Container> && std::equality_comparable<T>
class AddandremoveableContainerRef : public IAddandremoveableContainerRef<T>
{
public:
    explicit AddandremoveableContainerRef(Container &c LIFETIMEBOUND)
        : container_{ c } {}

    void add(T&& value) override {
        container_.emplace_back(std::move(value));
    }

    void remove(const T& value) override {
        auto it = std::find(container_.begin(), container_.end(), value);
        if (it != container_.end()) {
            container_.erase(it);
            //std::cout << "[Wrapper] Removed from " << typeid(Container).name() << "\n";
        }
    }

private:
    gsl::not_null<Container *> container_;
};

}

#endif