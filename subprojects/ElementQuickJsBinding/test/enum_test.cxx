#include "test_common.hxx"

#include <format>
#include <string>

// 枚举与脚本字符串的互转
//
// 绑定层为所有枚举统一提供名字互转（ElementQuickJsBinding/metapp_enum_metatype.hxx），
// 所以下面两个枚举都没有任何 metapp 特化：
//   * 脚本字符串按名字变成枚举实参（'HIGH' -> Level::HIGH）；
//   * 返回的枚举在脚本侧就是它的名字（Level::MID -> 'MID'）；
//   * 未知名字被拒绝，报错里带着枚举的类型名（MetaClass 由绑定层自动补上）；
//   * metapp 默认的「枚举 <-> 整数」互转没有被破坏。

enum class Level { LOW, MID, HIGH };

// 未作用域化的枚举走的是同一套。
enum Direction { NORTH, EAST, SOUTH, WEST };

// 带命名空间的枚举：类型名要连命名空间一起带，报错里才不会产生歧义。
namespace Outer { enum class Kind { BASE, EXTRA }; }

int main() {
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	qjs::Runtime rt;
	qjs::Context ctx{ rt };
	initJs(rt, ctx);

	Binding binding{ rt, ctx };

	Level lastLevel = Level::LOW;
	Direction lastDirection = NORTH;

	binding.regGlobalFunction("takeLevel",
		std::function<void(Level)>{ [&](Level level) { lastLevel = level; } });
	binding.regGlobalFunction("takeDirection",
		std::function<void(Direction)>{ [&](Direction direction) { lastDirection = direction; } });
	binding.regGlobalFunction("giveLevel",
		std::function<Level()>{ []() -> Level { return Level::MID; } });
	binding.regGlobalFunction("giveDirection",
		std::function<Direction()>{ []() -> Direction { return WEST; } });

	try_eval_module(ctx, rt, R"(
		takeLevel('HIGH');
		takeDirection('EAST');
		globalThis.levelName = giveLevel();
		globalThis.directionName = giveDirection();

		try {
			takeLevel('NOT_A_LEVEL');
			globalThis.unknownAccepted = true;
		} catch (e) {
			globalThis.unknownMessage = String(e);
		}
	)");

	const auto readGlobalInt = [&](const char *name) {
		JSValue value = JS_GetPropertyStr(ctx.ctx, ctx.global().v, name);
		std::int32_t result = 0;
		JS_ToInt32(ctx.ctx, &result, value);
		JS_FreeValue(ctx.ctx, value);
		return result;
	};
	const auto readGlobalString = [&](const char *name) {
		JSValue value = JS_GetPropertyStr(ctx.ctx, ctx.global().v, name);
		std::string result;
		if (JS_IsString(value)) {
			if (const char *text = JS_ToCString(ctx.ctx, value); text != nullptr) {
				result = text;
				JS_FreeCString(ctx.ctx, text);
			}
		}
		JS_FreeValue(ctx.ctx, value);
		return result;
	};
	const auto isUndefined = [&](const char *name) {
		JSValue value = JS_GetPropertyStr(ctx.ctx, ctx.global().v, name);
		const bool result = JS_IsUndefined(value);
		JS_FreeValue(ctx.ctx, value);
		return result;
	};

	// 1. 脚本字符串 -> 枚举实参。
	if (lastLevel != Level::HIGH) {
		std::println(stderr, "FAIL: takeLevel('HIGH') did not reach C++.");
		return 1;
	}
	if (lastDirection != EAST) {
		std::println(stderr, "FAIL: takeDirection('EAST') did not reach C++.");
		return 1;
	}
	std::println("a script string reached the C++ parameters: takeLevel('HIGH'), takeDirection('EAST')");

	// 2. 枚举返回值 -> 脚本字符串。
	if (const auto name = readGlobalString("levelName"); name != "MID") {
		std::println(stderr, "FAIL: giveLevel() produced \"{}\", expected \"MID\".", name);
		return 1;
	}
	if (const auto name = readGlobalString("directionName"); name != "WEST") {
		std::println(stderr, "FAIL: giveDirection() produced \"{}\", expected \"WEST\".", name);
		return 1;
	}
	std::println("an enum return value reached the script as its name: giveLevel() === \"MID\"");

	// 3. 未知名字被拒绝，且报错里带着枚举的类型名。
	if (!isUndefined("unknownAccepted")) {
		std::println(stderr, "FAIL: an unknown enum name was accepted.");
		return 1;
	}
	if (const auto message = readGlobalString("unknownMessage"); message.find("Level") == std::string::npos) {
		std::println(stderr, "FAIL: the error for an unknown enum name lacks the type name: {}", message);
		return 1;
	} else {
		std::println("an unknown enum name was rejected: {}", message);
	}

	// 4. metapp 默认的「枚举 <-> 整数」互转没有被替换掉。
	{
		const metapp::MetaType *levelType = metapp::getMetaType<Level>();
		if (levelType->getTypeKind() != metapp::tkEnum || !levelType->isEnum()) {
			std::println(stderr, "FAIL: Level is no longer an enum metatype.");
			return 1;
		}
		if (metapp::Variant{ Level::HIGH }.cast<int>().get<int>() != static_cast<int>(Level::HIGH)) {
			std::println(stderr, "FAIL: enum -> int stopped working.");
			return 1;
		}
		if (metapp::Variant{ static_cast<int>(Level::HIGH) }.cast(levelType).get<Level>() != Level::HIGH) {
			std::println(stderr, "FAIL: int -> enum stopped working.");
			return 1;
		}
	}

	// 5. 类型名由绑定层自动登记（供报错与 JS class name 使用）。
	{
		const metapp::MetaType *levelType = metapp::getMetaType<Level>();
		const metapp::MetaClass *metaClass = levelType->getMetaClass();
		if (metaClass == nullptr) {
			std::println(stderr, "FAIL: Level has no MetaClass.");
			return 1;
		}
		const auto name = metaClass->getType(levelType).getName();
		if (name != "Level") {
			std::println(stderr, "FAIL: the type name of Level is \"{}\", expected \"Level\".", name);
			return 1;
		}
		std::println("the type name of Level was registered automatically: \"{}\"", name);
	}

	// 6. 命名空间里的枚举走同一套；类型名连命名空间一起带，报错里才不会有歧义。
	{
		Outer::Kind lastKind = Outer::Kind::BASE;
		binding.regGlobalFunction("takeKind",
			std::function<void(Outer::Kind)>{ [&](Outer::Kind kind) { lastKind = kind; } });

		try_eval_module(ctx, rt, "takeKind('EXTRA');");
		if (lastKind != Outer::Kind::EXTRA) {
			std::println(stderr, "FAIL: takeKind('EXTRA') did not reach C++.");
			return 1;
		}

		const metapp::MetaType *kindType = metapp::getMetaType<Outer::Kind>();
		const auto name = kindType->getMetaClass()->getType(kindType).getName();
		if (name != "Outer::Kind") {
			std::println(stderr, "FAIL: the type name of Outer::Kind is \"{}\", expected \"Outer::Kind\".", name);
			return 1;
		}
		std::println("a namespaced enum works the same, its type name is \"{}\"", name);
	}

	(void)readGlobalInt;

	std::println("PASS");
	return 0;
}
