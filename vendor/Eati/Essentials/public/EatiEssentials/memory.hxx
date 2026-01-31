/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */

#ifndef ESS_MEMORY_HXX
#define ESS_MEMORY_HXX
#include <memory>
#include <gsl/gsl>

namespace Essentials::Memory
{

template<class T, class D = std::default_delete<T>>
using Box = std::unique_ptr<T, D>;

template<class T>
using Weak = std::weak_ptr<T>;

#define makeBox std::make_unique

template<class T>
Box<T> makeBoxA(gsl::owner<T *> ptr) {
	return Box<T>(ptr);
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define newBox(...) Essentials::Memory::makeBoxA(new __VA_ARGS__)

template<class T>
using Rc = std::shared_ptr<T>;

template<class T>
Rc<T> makeRcA(T *ptr) {
	return Rc<T>(ptr);
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define newRc(...) makeRcA(new __VA_ARGS__)

}

namespace ESSM = Essentials::Memory;

#endif // !ESS_MEMORY_HXX
