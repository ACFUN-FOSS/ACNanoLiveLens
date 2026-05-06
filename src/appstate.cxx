#include "appstate.hxx"

std::optional<AppState> globalAppState;

void initAppState(AppState &&appState){
	globalAppState.emplace(std::move(appState));
}

AppState &getAppState() {
	assert(globalAppState.has_value() && "AppState is not initialized yet!");
    return *globalAppState;
}

metapp::MetaRepo &getGlobalMetaRepo() {
	static metapp::MetaRepo metaRepo;
	return metaRepo;
}