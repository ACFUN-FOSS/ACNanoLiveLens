#ifndef _H_7QIVK
#define _H_7QIVK

#include <chrono>
#include <EatiEssentials/memory.hxx>

namespace ElementEngine::QJSBinding
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
	{obj.lifetimeInformant} -> std::same_as<LifetimeInformant>;
};

template <LifetimeAware T>
ESSM::Rc<LifetimeInformant::LifetimeInfo> getLifetimeInfo(T &lifetimeAware) {
	return lifetimeAware.lifetimeInformant.shareLifetimeInfo();
}

}

#endif // !Guard
