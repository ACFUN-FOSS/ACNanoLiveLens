#ifndef EJS_BINDING_METAPP_ENUM_METATYPE_HXX
#define EJS_BINDING_METAPP_ENUM_METATYPE_HXX

// Enum <-> script string marshalling for the QuickJS binding.
//
// Enums cross the scripting boundary by name: a JS string such as 'RELOAD'
// converts to the matching enum value, and returning an enum to the script
// yields its name. No enum needs any binding code of its own -- including
// this header (binding.hxx does) makes every enum behave that way.
//
// This works because metapp resolves its type description through
// DeclareMetaType<T>, so a partial specialization covering "T is an enum"
// replaces the default enum metatype for every enum at once. The
// specialization derives from metapp's own enum metatype
// (DeclareMetaTypeBase<T, enable_if<is_enum<T>>>), so metapp's default
// enum <-> integer casts stay intact and only the string direction is added.
//
// Notes:
//   - value <-> name mapping is delegated to reflectcpp (rfl::string_to_enum
//     / rfl::enum_to_string, enchantum underneath), so adding or renaming an
//     enumerator requires no change here;
//   - being a partial specialization of DeclareMetaType, this must be
//     visible in every TU that instantiates DeclareMetaType of an enum
//     (ODR) -- the same reason as metapp_view_metatypes.hxx, which
//     binding.hxx includes right next to this one;
//   - the MetaClass is provided so the type name shows up in binding
//     diagnostics; without it a binding error can only print an address.
//     The name itself comes from reflectcpp's compile-time type name
//     (rfl::type_name_t), so it is the enum's real name -- namespaces
//     included -- and no literal has to be kept in sync.

#include <metapp/metatype.h>
#include <metapp/cast.h>
#include <metapp/interfaces/metaclass.h>
#include <metapp/metatypes/enum.h>
#include <metapp/metatypes/std_string.h>

#include <rfl/enums.hpp>
#include <rfl/type_name_t.hpp>

#include <string>
#include <type_traits>

namespace metapp
{

// Replaces metapp's default enum metatype for every enum type. Deriving from
// it keeps tkEnum and the integral casts; the name is what is added.
template <typename T>
struct DeclareMetaType<T, std::enable_if_t<std::is_enum_v<T>>>
	: DeclareMetaTypeBase<T>
{
private:
	using Base = DeclareMetaTypeBase<T>;
	using Value = std::remove_cv_t<T>;

public:
	using UpType = typename Base::UpType;
	static constexpr TypeKind typeKind = Base::typeKind;

	// An enum value -> its name for a std::string target; every other target
	// keeps metapp's default behaviour (the integral casts).
	static bool cast(Variant * result, const Variant * fromVar, const MetaType * toMetaType)
	{
		if (getNonReferenceMetaType(toMetaType)->getTypeKind() == tkStdString) {
			if (result != nullptr)
				*result = Variant{ rfl::enum_to_string(fromVar->get<Value>()) };
			return true;
		}
		return Base::cast(result, fromVar, toMetaType);
	}

	// A name -> the matching enum value; every other source keeps metapp's
	// default behaviour. An unknown name fails the cast, which lets the
	// binding report it as a marshalling error.
	static bool castFrom(Variant * result, const Variant * fromVar, const MetaType * fromMetaType)
	{
		if (getNonReferenceMetaType(fromMetaType)->getTypeKind() == tkStdString) {
			const auto parsed = rfl::string_to_enum<Value>(fromVar->get<std::string>());
			if (!parsed)
				return false;
			if (result != nullptr)
				*result = Variant{ *parsed };
			return true;
		}
		return Base::castFrom(result, fromVar, fromMetaType);
	}

	// The name has to live on the MetaClass itself: that is the only place
	// binding.cxx's getTypeName reads.
	static const MetaClass * getMetaClass()
	{
		static const MetaClass metaClass{
			getMetaType<T>(),
			[](MetaClass &mc) {
				// rfl::type_name_t<T> is rfl::Literal<the type name of T>;
				// .str() unwraps it. One field, hence the default-constructed
				// literal names T itself.
				mc.registerType<T>(rfl::type_name_t<T>{}.str());
			}
		};
		return &metaClass;
	}
};

} // namespace metapp

#endif // EJS_BINDING_METAPP_ENUM_METATYPE_HXX
