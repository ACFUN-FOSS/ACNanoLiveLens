#include <print>
#include "ElementQuickJsBinding/binding.hxx"

using namespace ElementEngine::QJSBinding;

class LifetimeAwareTestClass
{
public:
    LifetimeInformant lifetimeInformant;
private:
};


int main() {
    regTypeStatic<LifetimeAwareTestClass>();
}