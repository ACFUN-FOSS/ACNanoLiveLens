#ifndef NANOLIVELENS_RMLUI_ELEMENT_HXX
#define NANOLIVELENS_RMLUI_ELEMENT_HXX

class RmlUIElement : public Rml::Element
{
public:
    RmlUIElement(const std::string_view tag, bool isWindowElement);
    RmlUIElement(const RmlUIElement &) = delete;
    RmlUIElement(RmlUIElement &&) = delete;
    RmlUIElement &operator=(const RmlUIElement &) = delete;
    RmlUIElement &operator=(RmlUIElement &&) = delete;
    ~RmlUIElement() = default;

    virtual void reload();

	bool getIsWindowElement() const;

protected:
    void OnUpdate() override;
    void ProcessDefaultAction(Rml::Event &event) override;

    virtual void onUpdate();
    virtual void processDefaultAction(Rml::Event &event);

	bool isWindowElement = false;
};

#endif //NANOLIVELENS_RMLUI_ELEMENT_HXX
