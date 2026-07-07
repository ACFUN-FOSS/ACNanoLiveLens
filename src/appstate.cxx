#include "appstate.hxx"

namespace App {

std::optional<State> globalState;

void initState(State &&state){
	globalState.emplace(std::move(state));
}

State &getState() {
	assert(globalState.has_value() && "App::State is not initialized yet!");
    return *globalState;
}

metapp::MetaRepo &getGlobalMetaRepo() {
	static metapp::MetaRepo metaRepo;
	return metaRepo;
}

void die(int code) {
	std::println(
		"=========="
		" DIE! code={} "
		"==========",
		code);
	Rml::Shutdown();
	std::_Exit(code);
}

}
