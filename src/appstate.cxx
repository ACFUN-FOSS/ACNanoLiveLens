#include <appstate.hxx>

AppState &getAppState() {
    static AppState appState;
    return appState;
}