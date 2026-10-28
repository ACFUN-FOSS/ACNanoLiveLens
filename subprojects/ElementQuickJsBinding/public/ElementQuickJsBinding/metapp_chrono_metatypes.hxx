#ifndef EJS_BINDING_METAPP_CHRONO_METATYPES_HXX
#define EJS_BINDING_METAPP_CHRONO_METATYPES_HXX

// Chrono <-> script marshalling for the QuickJS binding.
//
// Every std::chrono::time_point crosses into the script as a JS Date, every
// std::chrono::duration as a JS number (a count of milliseconds). This holds
// for standalone values and for chrono members of registered types alike: a
// time_point member reads as a Date and accepts a Date back when written.
// Neither chrono family needs any registration, translator or per-type binding
// code -- including this header (which binding.hxx does) is all it takes.
//
// The mechanism mirrors the enum marshalling in metapp_enum_metatype.hxx:
// metapp resolves its type description through DeclareMetaType<T>, so a partial
// specialization covering "T is a chrono type" replaces the default description
// of every such type at once. Deriving from metapp's own default
// (CommonDeclareMetaType<T>) keeps tkObject and the copy/construct machinery
// untouched; the declaration adds exactly two things:
//
//   * a MetaUser marker (metapp's type-level escape hatch, read through
//     MetaType::getMetaUser()) naming the chrono family, so the binding can
//     pick Date or number without knowing the concrete clock/period;
//   * casts between the value and a count of milliseconds -- to an integral
//     target the exact milliseconds (the resolution of a JS Date), to a
//     floating point target the milliseconds without truncating sub-
//     millisecond precision, and from any arithmetic source back to the value.
//
// Milliseconds is the canonical unit because JS dates and timers count
// milliseconds since the epoch.
//
// Note: being a partial specialization of DeclareMetaTypeBase, this must be
// visible in every TU instantiating DeclareMetaType of a chrono type (ODR) --
// the same reason as metapp_view_metatypes.hxx, which binding.hxx includes
// right next to this one.

#include <metapp/metatype.h>
#include <metapp/cast.h>
#include <metapp/typekind.h>
#include <metapp/variant.h>

#include <chrono>
#include <cstdint>

namespace ElementEngine::QJSBinding
{

// Which chrono family a type belongs to. A chrono metatype hands out the address
// of one of the two markers below as its MetaUser, and the pointee carries the
// family. These are inline variables, so their address is unique program-wide
// and the binding can compare against them directly.
enum class ChronoKind
{
	timePoint,
	duration,
};

namespace detail
{

inline constexpr ChronoKind chronoTimePointMarker = ChronoKind::timePoint;
inline constexpr ChronoKind chronoDurationMarker = ChronoKind::duration;

// Milliseconds since the epoch: the only form chrono values take on the script
// side. duration_cast covers the sub-millisecond periods clocks may use (MSVC's
// system_clock ticks at 100 ns).
template <typename Duration>
std::chrono::duration<double, std::milli> toMilliseconds(const Duration & duration) {
	return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(duration);
}

// The milliseconds handed over by the script: a JS number, possibly fractional
// (a Date's getTime() is integral, a duration may be written as 1.5).
inline std::chrono::duration<double, std::milli> fromMilliseconds(const metapp::Variant & millisecondsVar) {
	return std::chrono::duration<double, std::milli>{ millisecondsVar.cast<double>().get<double>() };
}

} // namespace detail

// Reads the chrono marker of a type: returns true and sets kind for a time point
// or a duration, false (leaving kind untouched) for every other type.
inline bool getChronoKind(const metapp::MetaType & type, ChronoKind & kind)
{
	const void *const metaUser = type.getMetaUser();
	if (metaUser == &detail::chronoTimePointMarker || metaUser == &detail::chronoDurationMarker) {
		kind = *static_cast<const ChronoKind *>(metaUser);
		return true;
	}
	return false;
}

} // namespace ElementEngine::QJSBinding

namespace metapp
{

namespace ejsChrono = ElementEngine::QJSBinding::detail;

// Any time point -> milliseconds since its epoch.
template <typename Clock, typename Duration>
struct DeclareMetaTypeBase<std::chrono::time_point<Clock, Duration>>
	: CommonDeclareMetaType<std::chrono::time_point<Clock, Duration>>
{
private:
	using T = std::chrono::time_point<Clock, Duration>;
	using Base = CommonDeclareMetaType<T>;

public:
	static const void * getMetaUser() {
		return &ElementEngine::QJSBinding::detail::chronoTimePointMarker;
	}

	static bool cast(Variant * result, const Variant * fromVar, const MetaType * toMetaType) {
		if (getNonReferenceMetaType(toMetaType)->isArithmetic()) {
			const auto & value = *static_cast<const T *>(fromVar->getAddress());
			const auto milliseconds = ejsChrono::toMilliseconds(value.time_since_epoch());
			if (result != nullptr) {
				*result = getNonReferenceMetaType(toMetaType)->isIntegral()
					? Variant{ std::chrono::duration_cast<std::chrono::milliseconds>(milliseconds).count() }
					: Variant{ milliseconds.count() };
			}
			return true;
		}
		return Base::cast(result, fromVar, toMetaType);
	}

	static bool castFrom(Variant * result, const Variant * fromVar, const MetaType * fromMetaType) {
		if (getNonReferenceMetaType(fromMetaType)->isArithmetic()) {
			if (result != nullptr)
				*result = Variant{ T{ std::chrono::duration_cast<Duration>(
					ejsChrono::fromMilliseconds(*fromVar)) } };
			return true;
		}
		return false;
	}
};

// Any duration -> a count of milliseconds.
template <typename Rep, typename Period>
struct DeclareMetaTypeBase<std::chrono::duration<Rep, Period>>
	: CommonDeclareMetaType<std::chrono::duration<Rep, Period>>
{
private:
	using T = std::chrono::duration<Rep, Period>;
	using Base = CommonDeclareMetaType<T>;

public:
	static const void * getMetaUser() {
		return &ElementEngine::QJSBinding::detail::chronoDurationMarker;
	}

	static bool cast(Variant * result, const Variant * fromVar, const MetaType * toMetaType) {
		if (getNonReferenceMetaType(toMetaType)->isArithmetic()) {
			const auto & value = *static_cast<const T *>(fromVar->getAddress());
			const auto milliseconds = ejsChrono::toMilliseconds(value);
			if (result != nullptr) {
				*result = getNonReferenceMetaType(toMetaType)->isIntegral()
					? Variant{ std::chrono::duration_cast<std::chrono::milliseconds>(milliseconds).count() }
					: Variant{ milliseconds.count() };
			}
			return true;
		}
		return Base::cast(result, fromVar, toMetaType);
	}

	static bool castFrom(Variant * result, const Variant * fromVar, const MetaType * fromMetaType) {
		if (getNonReferenceMetaType(fromMetaType)->isArithmetic()) {
			if (result != nullptr)
				*result = Variant{ std::chrono::duration_cast<T>(
					ejsChrono::fromMilliseconds(*fromVar)) };
			return true;
		}
		return false;
	}
};

} // namespace metapp

#endif // EJS_BINDING_METAPP_CHRONO_METATYPES_HXX
