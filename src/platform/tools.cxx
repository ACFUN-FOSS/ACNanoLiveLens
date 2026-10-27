#include "tools.hxx"

stdf::path getExecPath() {
    return boost::dll::program_location().string();
}