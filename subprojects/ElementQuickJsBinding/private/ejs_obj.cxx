#include "ElementQuickJsBinding/ejs_obj.hxx"

namespace ElementEngine::QJSBinding
{

std::optional<std::string> EJSObjOpaque::getLifetimeInvalidReason() {
	if (lifetimeInfoOfCppObj->isMovedAway) {
		auto elapsedStr = [&]() -> std::string {
			if (!lifetimeInfoOfCppObj->movedAwayTime)
				return std::string{};
			auto elapsed = std::chrono::steady_clock::now() - lifetimeInfoOfCppObj->movedAwayTime.value();
			auto mins = std::chrono::duration_cast<std::chrono::minutes>(elapsed);
			auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed - mins);
			return std::format("({}m{}s)", mins.count(), secs.count());
		}();

		return std::format(
			"Attempt to access a C++ object that has been already moved away. "
			"The object has been moved away before {}",
			elapsedStr
		);
	}

	if (!lifetimeInfoOfCppObj->isAlive) {
		auto elapsedStr = [&]() -> std::string {
			if (!lifetimeInfoOfCppObj->deathTime)
				return std::string{};
			auto elapsed = std::chrono::steady_clock::now() - lifetimeInfoOfCppObj->deathTime.value();
			auto mins = std::chrono::duration_cast<std::chrono::minutes>(elapsed);
			auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed - mins);
			return std::format("({}m{}s)", mins.count(), secs.count());
		}();
		return std::format(
			"Attempt to access a C++ object that has been already dead. "
			"The object has been dead before {}",
			elapsedStr
		);
	}

	return { };
}

static std::any *getEJSOpaqueWrapperAny(JSValue jsvalue) {
	auto opaque = JS_GetOpaque(jsvalue, JS_GetClassID(jsvalue));
	if (!opaque)
		return nullptr;

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	auto any = reinterpret_cast<std::any *>(opaque);
	return any;
}

EJSObjOpaque *getEJSObjOpaque(JSValue jsvalue) {
	auto any = getEJSOpaqueWrapperAny(jsvalue);
	if (!any || any->type() != typeid(EJSObjOpaque))
		return nullptr;

	return std::any_cast<EJSObjOpaque>(any);
}

JSValue makeEJSObj(JSContext &ctx, int classID, EJSObjOpaque opaque) {
	JSValue jsobj = JS_NewObjectClass(&ctx, classID);
	gsl::owner<std::any *> realOpaque{ new std::any{
		opaque
	} };
	return jsobj;
}

void freeEJSObjOpaque(JSValue jsvalue) {
	gsl::owner<std::any *> opaqueWrapperAny{ getEJSOpaqueWrapperAny(jsvalue) };
	if (!opaqueWrapperAny)
		throw std::invalid_argument{ "Not a EJSObj" };

	delete opaqueWrapperAny;
}

}
