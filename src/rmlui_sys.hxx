#ifndef NANOLIVELENS_RMLUISYSTEM_HXX
#define NANOLIVELENS_RMLUISYSTEM_HXX


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
