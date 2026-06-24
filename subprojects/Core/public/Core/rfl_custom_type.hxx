#ifndef NANOLIVELENS_CORE_RFL_CUSTOM_TYPE_HXX
#define NANOLIVELENS_CORE_RFL_CUSTOM_TYPE_HXX

#include <chrono>
#include <rfl.hpp>

namespace rfl {

template <>
struct Reflector<std::chrono::system_clock::time_point> {
	using ReflType = long long;

	static std::chrono::system_clock::time_point to(const ReflType& v) noexcept {
		return std::chrono::system_clock::time_point{ std::chrono::milliseconds{ v } };
	}

	static ReflType from(const std::chrono::system_clock::time_point& v) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			v.time_since_epoch()
		).count();
	}
};

}

#endif
