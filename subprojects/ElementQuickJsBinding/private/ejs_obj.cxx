#include "ElementQuickJsBinding/ejs_obj.hxx"
#include <chrono>
#include <format>
#include <set>
#include <utility>

namespace ElementEngine::QJSBinding
{

std::optional<std::string> EJSObjOpaque::getLifetimeInvalidReason() {
	if (!lifetimeInfoOfCppObj)
		return { };

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

static std::set<JSClassID> &getEJSClassIDs() {
	static std::set<JSClassID> ejsClassIDs;
	return ejsClassIDs;
}

static bool isEJSClassID(JSClassID classID) {
	return getEJSClassIDs().contains(classID);
}

static void registerEJSClassID(JSClassID classID) {
	getEJSClassIDs().insert(classID);
}

EJSObjOpaque *getEJSObjOpaque(JSValue jsvalue) {
	auto classID = JS_GetClassID(jsvalue);
	if (!isEJSClassID(classID))
		return nullptr;

	return static_cast<EJSObjOpaque *>(JS_GetOpaque(jsvalue, classID));
}

JSValue makeEJSObj(JSContext &ctx, int classID, EJSObjOpaque opaque) {
	JSValue jsobj = JS_NewObjectClass(&ctx, classID);
	if (JS_IsException(jsobj))
		return jsobj;

	registerEJSClassID(classID);

	gsl::owner<EJSObjOpaque *> realOpaque{ new EJSObjOpaque{ std::move(opaque) } };
	JS_SetOpaque(jsobj, realOpaque);
	return jsobj;
}

void freeEJSObjOpaque(JSValue jsvalue) {
	gsl::owner<EJSObjOpaque *> opaque{ getEJSObjOpaque(jsvalue) };
	if (!opaque)
		return;

	delete opaque;
}

}
