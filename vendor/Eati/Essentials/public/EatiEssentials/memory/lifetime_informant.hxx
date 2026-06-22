/*
 * Copyright (c) 2026 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */

#ifndef ESS_LIFETIME_INFORMANT_HXX
#define ESS_LIFETIME_INFORMANT_HXX

#include <chrono>
#include <concepts>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <EatiEssentials/memory/memory.hxx>

namespace Essentials::Memory
{

class LifetimeInformant
{
public:
	struct LifetimeInfo
	{
		std::chrono::steady_clock::time_point createTime;
		std::optional<std::chrono::steady_clock::time_point> deathTime;
		std::optional<std::chrono::steady_clock::time_point> movedAwayTime;
		bool isAlive = true;
		bool isMovedAway = false;
	};

	LifetimeInformant();
	~LifetimeInformant();
	LifetimeInformant(const LifetimeInformant &);
	LifetimeInformant(LifetimeInformant &&) noexcept;
	LifetimeInformant &operator=(const LifetimeInformant &);
	LifetimeInformant &operator=(LifetimeInformant &&) noexcept;

	ESSM::Rc<LifetimeInfo> shareLifetimeInfo();

private:
	ESSM::Rc<LifetimeInfo> info;
};


template <typename T>
concept LifetimeAware = requires(T obj)
{
	{obj.lifetimeInformant} -> std::same_as<LifetimeInformant &>;
};

template <LifetimeAware T>
ESSM::Rc<LifetimeInformant::LifetimeInfo> getLifetimeInfo(T &lifetimeAware) {
	return lifetimeAware.lifetimeInformant.shareLifetimeInfo();
}

class NullPointerExcp : public std::runtime_error
{
public:
	NullPointerExcp(std::string_view className);
};

class TargetGone : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

template <LifetimeAware T>
class LifetimeAwareWRef
{
private:
	T *ptr = nullptr;
	ESSM::Rc<LifetimeInformant::LifetimeInfo> li;
public:
	LifetimeAwareWRef() = default;
	LifetimeAwareWRef(T &ref)
		: ptr{ &ref },
		  li{ getLifetimeInfo(ref) } { }
	~LifetimeAwareWRef() = default;
	LifetimeAwareWRef(const LifetimeAwareWRef& that) = default;
	LifetimeAwareWRef(LifetimeAwareWRef&& that) = default;
	LifetimeAwareWRef &operator=(const LifetimeAwareWRef& that) = default;
	LifetimeAwareWRef &operator=(LifetimeAwareWRef&& that) = default;

	T *operator->() {
		if (!ptr)
			throw NullPointerExcp{ "LifetimeAwareWRef" };
		if (li->isMovedAway || !li->isAlive)
			throw TargetGone{ "The target of the reference is gone." };
			return ptr;
	}

	bool isValid() {
		return ptr != nullptr;
	}

	operator bool() {
		return isValid();
	}

	T &operator*() {
		return *operator->();
	}
};

}

#endif // !Guard
