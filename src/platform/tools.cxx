#include "tools.hxx"
#include <boost/dll.hpp>

stdf::path getExecPath() {
    return boost::dll::program_location().string();
}