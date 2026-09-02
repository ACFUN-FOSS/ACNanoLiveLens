#ifndef NANOLIVELENS_RMLUI_ELEMENT_HXX
#define NANOLIVELENS_RMLUI_ELEMENT_HXX

#include "rmlui_sys.hxx"

class RmlUIElement : public Rml::Element
{
public:
    RmlUIElement(const std::string_view tag, bool isWindowElement);
    RmlUIElement(const RmlUIElement &) = delete;
    RmlUIElement(RmlUIElement &&) = delete;
    RmlUIElement &operator=(const RmlUIElement &) = delete;
    RmlUIElement &operator=(RmlUIElement &&) = delete;
    ~RmlUIElement() = default;

    // Rebuild element-owned content after the document has been replaced.
    virtual void reload();
    void reloadStyles();

	bool getIsWindowElement() const;

protected:
    void OnUpdate() override;
    void ProcessDefaultAction(Rml::Event &event) override;

    virtual void onMounted();
    virtual void onUpdate();
    virtual void processDefaultAction(Rml::Event &event);

	bool isWindowElement = false;

private:
	void ensureMounted();

	bool mounted_ = false;
};

void registerCustomElements(RmlUISystem &rmlui);

#endif //NANOLIVELENS_RMLUI_ELEMENT_HXX
