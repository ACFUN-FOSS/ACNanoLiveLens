/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */
#ifndef NANOLIVELENS_CONTAINER_LEASE_HXX
#define NANOLIVELENS_CONTAINER_LEASE_HXX
#include "EatiEssentials/container_and_view_and_ranges/addandremoveable_container_ref.hxx"
#include "EatiEssentials/memory.hxx"

namespace Essentials::ContainerAndView
{

/**
 * @class ContainerLease
 * @brief A class for managing the “lease” of a value within a container.
 *
 * This class is designed to manage the lifecycle of a value within a container, ensuring that
 * the value is added to the container when the `ContainerLease` object is created and removed
 * from the container when the `ContainerLease` object is destroyed. The class uses a wrapper
 * to interact with the container, which must meet the `AddandremoveableContainer` concept.
 * The `ContainerLease` class is non-copyable and non-assignable, enforcing a strict ownership
 * model.
 *
 * @tparam T The type of the value being managed.
 */
template <typename T>
class ContainerLease {
public:
    template <typename Container>
        requires AddandremoveableContainer<Container> && std::same_as<typename Container::value_type, T>
    ContainerLease(Container& container LIFETIMEBOUND, T value) 
        : value_{ value },
          wrapper_{ newBox(AddandremoveableContainerRef<T, Container>{ container }) } {
        // 在运行时创建具体的包装器
        wrapper_->add(std::move(value));
    }

    ~ContainerLease() {
        if (wrapper_) {
            wrapper_->remove(value_);
        }
    }

    ContainerLease(ContainerLease &&) = default;
    ContainerLease(const ContainerLease &) = delete;
    ContainerLease& operator=(const ContainerLease &) = delete;
    ContainerLease& operator=(ContainerLease &&) = default;

private:
    T value_;
    Memory::Box<IAddandremoveableContainerRef<T>> wrapper_; // 运行时指向具体的容器逻辑
};

/**
 * @class AssoContainerLease
 * @brief A class for managing the “lease” of a key-value pair within an associative container.
 *
 * Associate-container-version of ContainerLease.
 *
 * @tparam TKey The type of the key being managed.
 * @tparam TVal The type of the mapped value being managed.
 */
template <typename TKey, typename TVal>
class AssoContainerLease {
public:
    template <typename Container>
        requires AddandremoveableAssoContainer<Container, TKey, TVal>
    AssoContainerLease(Container& container LIFETIMEBOUND, TKey key, TVal value)
        : key_{ key },
          wrapper_{ newBox(AddandremoveableAssoContainerRef<TKey, TVal, Container>{ container }) } {
        wrapper_->add(std::move(key), std::move(value));
    }

    ~AssoContainerLease() {
        if (wrapper_) {
            wrapper_->remove(key_);
        }
    }

    AssoContainerLease(AssoContainerLease &&) = default;
    AssoContainerLease(const AssoContainerLease &) = delete;
    AssoContainerLease& operator=(const AssoContainerLease &) = delete;
    AssoContainerLease& operator=(AssoContainerLease &&) = default;

private:
    TKey key_;
    Memory::Box<IAddandremoveableAssoContainerRef<TKey, TVal>> wrapper_;
};

}


#endif //NANOLIVELENS_CONTAINER_LEASE_HXX
