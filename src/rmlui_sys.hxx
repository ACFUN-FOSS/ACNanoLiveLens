#ifndef NANOLIVELENS_RMLUISYSTEM_HXX
#define NANOLIVELENS_RMLUISYSTEM_HXX
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <EatiEssentials/memory.hxx>

class RmlUISystem
{
public:
    RmlUISystem(Rml::SystemInterface &sysItfc, Rml::RenderInterface &renderItfc);
    ~RmlUISystem();
    RmlUISystem(const RmlUISystem &) = delete;
    RmlUISystem(RmlUISystem &&) = delete;
    RmlUISystem &operator=(const RmlUISystem &) = delete;
    RmlUISystem &operator=(RmlUISystem &&) = delete;

    void regElement(std::string_view &&name, ESSM::Box<Rml::ElementInstancer> &&instancer);
private:
    struct RmlUILifetimeThings;
    ESSM::Box<RmlUILifetimeThings> rmlUiLifetimeThings_;
};


#endif //NANOLIVELENS_RMLUISYSTEM_HXX