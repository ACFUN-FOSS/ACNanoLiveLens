#include "ElementQuickJsBinding/lifetime_informant.hxx"

using namespace Essentials::Memory;

namespace ElementEngine::QJSBinding
{

LifetimeInformant::LifetimeInformant()
	: info{ newBox(
		LifetimeInfo{ std::chrono::steady_clock::now() })} {
}
LifetimeInformant::~LifetimeInformant() {
	info->isAlive = false;
	info->deathTime = std::chrono::steady_clock::now();
}
	
LifetimeInformant::LifetimeInformant(const LifetimeInformant &other)
	: LifetimeInformant{ } {
}

LifetimeInformant::LifetimeInformant(LifetimeInformant &&other) noexcept
	: LifetimeInformant{ } {
	other.info->isMovedAway = true;
	other.info->movedAwayTime = std::chrono::steady_clock::now();
}

LifetimeInformant &LifetimeInformant::operator=(const LifetimeInformant &other) {
	if (this == &other)
		return *this;
	return *this;
}

LifetimeInformant &LifetimeInformant::operator=(LifetimeInformant &&other) noexcept {
	if (this == &other)
		return *this;
	return *this;
}

Rc<LifetimeInformant::LifetimeInfo> LifetimeInformant::shareLifetimeInfo() {
	return info;
}

}
