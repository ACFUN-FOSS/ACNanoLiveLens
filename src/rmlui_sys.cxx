#include "rmlui_sys.hxx"
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi_Backend.h>

struct RmlUISystem::RmlUILifetimeThings
{
    std::vector<ESSM::Box<Rml::ElementInstancer>> instancers_;
};

RmlUISystem::RmlUISystem(Rml::SystemInterface &sysItfc, Rml::RenderInterface &renderItfc)
    : rmlUiLifetimeThings_{new RmlUILifetimeThings{}}
{
    Rml::SetSystemInterface(&sysItfc);
    Rml::SetRenderInterface(&renderItfc);
    Rml::Initialise();
}

RmlUISystem::~RmlUISystem() {
    Backend::OnRmlShutdown();
    Rml::Shutdown();
    rmlUiLifetimeThings_.reset();
}

void RmlUISystem::regElement(std::string_view &&name, ESSM::Box<Rml::ElementInstancer> &&instancer) {
    rmlUiLifetimeThings_->instancers_.push_back(std::move(instancer));
    Rml::Factory::RegisterElementInstancer(name.data(), rmlUiLifetimeThings_->instancers_.back().get());
}


