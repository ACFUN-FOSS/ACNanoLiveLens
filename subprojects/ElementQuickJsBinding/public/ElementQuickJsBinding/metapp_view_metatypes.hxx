#ifndef EJS_BINDING_METAPP_VIEW_METATYPES_HXX
#define EJS_BINDING_METAPP_VIEW_METATYPES_HXX

// Type-level whitelist of C++ non-owning view types (std::span,
// std::string_view) for the QuickJS binding.
//
// metapp ships no metatypes for these C++20 views. These declarations make
// them first-class reflection citizens so the structural container support
// in binding.cxx can recognize and handle them:
//
//   - std::span<T, Extent> becomes MetaIndexable (read/write, never
//     resizable). It is marked with TypeKind tkUser so the binding can
//     detect "a view type passed by value": a by-value span still points
//     into foreign memory, so transplanting it into the JS VM can never be
//     safe and must be refused (views are only exposed as live twins).
//
//   - std::string_view gains castFrom support for std::string and
//     const char *, so JS strings can be marshalled into string_view
//     parameters (metapp casts via static_cast, which string_view provides).

#include <metapp/metatype.h>
#include <metapp/cast.h>
#include <metapp/exception.h>
#include <metapp/typekind.h>
#include <metapp/interfaces/metaindexable.h>
#include <metapp/implement/internal/util_i.h>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace metapp {

template <typename T, std::size_t Extent>
struct DeclareMetaTypeBase<std::span<T, Extent>>
{
	using UpType = TypeList<T>;
	// Marker for non-owning view types, consulted by the binding.
	static constexpr TypeKind typeKind = tkUser;

	static const MetaIndexable * getMetaIndexable() {
		static MetaIndexable metaIndexable(
			&spanGetSizeInfo,
			&spanGetValueType,
			nullptr, // a span is never resizable
			&spanGet,
			&spanSet
		);
		return &metaIndexable;
	}

private:
	using SpanType = std::span<T, Extent>;

	static MetaIndexable::SizeInfo spanGetSizeInfo(const Variant & indexable) {
		auto sizeInfo = MetaIndexable::SizeInfo{ indexable.get<const SpanType &>().size() };
		sizeInfo.setResizable(false);
		return sizeInfo;
	}

	static const MetaType * spanGetValueType(const Variant & /*indexable*/, const std::size_t /*index*/) {
		return getMetaType<T>();
	}

	static Variant spanGet(const Variant & indexable, const std::size_t index) {
		auto & span = indexable.get<SpanType &>();
		if (index >= span.size())
			raiseException<OutOfRangeException>();
		return Variant::reference(span[index]);
	}

	static void spanSet(const Variant & indexable, const std::size_t index, const Variant & value) {
		auto & span = indexable.get<SpanType &>();
		if (index >= span.size())
			raiseException<OutOfRangeException>();
		internal_::assignValue(span[index],
			value.cast<T &>().template get<T &>());
	}
};

template <>
struct DeclareMetaTypeBase<std::string_view>
	: CastFromTypes<std::string_view, TypeList<std::string, const char *>>
{ };

} // namespace metapp

#endif // EJS_BINDING_METAPP_VIEW_METATYPES_HXX
