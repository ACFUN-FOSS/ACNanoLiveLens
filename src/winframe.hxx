#ifndef NANOLIVELENS_WINFRAME_HXX
#define NANOLIVELENS_WINFRAME_HXX
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/XMLNodeHandler.h>

#include <utility>

#include "rmlui_sys.hxx"
#include "rmluipp.hxx"



class WinFrame : public Rml::Element
{
public:
    WinFrame(const std::string_view tag);
    static void reg(RmlUISystem &rmlui);
private:
	void OnUpdate() override;
	void ProcessDefaultAction(Rml::Event &event) override;

    // Dragging
    bool isDragging_ = false;
    Rml::Vector2i mousePosWhenBeginDrag_;

    SimpleEventListenerManager eventListenerMan_{ *this };

};


#endif //NANOLIVELENS_WINFRAME_HXX
