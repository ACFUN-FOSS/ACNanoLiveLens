#include "EatiEssentials/memory/moved_flag.hxx"

namespace Essentials::Memory
{

MovedFlag::MovedFlag() noexcept : moved_{ false } {}

MovedFlag::MovedFlag(const MovedFlag &) noexcept : moved_{ false } {}

MovedFlag::MovedFlag(MovedFlag &&other) noexcept : moved_{ false } {
	other.moved_ = true;
}

MovedFlag &MovedFlag::operator=(const MovedFlag &) noexcept {
	moved_ = false;
	return *this;
}

MovedFlag &MovedFlag::operator=(MovedFlag &&other) noexcept {
	if (this != &other) {
		moved_ = false;
		other.moved_ = true;
	}
	return *this;
}

MovedFlag::operator bool() noexcept {
	return moved();
}

[[nodiscard]] bool MovedFlag::moved() const noexcept {
	return moved_;
}

void MovedFlag::reset() noexcept {
	moved_ = false;
}

}
