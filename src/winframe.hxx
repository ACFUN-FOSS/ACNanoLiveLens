#ifndef NANOLIVELENS_WINFRAME_HXX
#define NANOLIVELENS_WINFRAME_HXX

#include "rmlui_element.hxx"
#include "rmlui_sys.hxx"
#include "rmluipp.hxx"


class WinFrame : public RmlUIElement
{
public:
	WinFrame(const std::string_view tag);
	WinFrame(const WinFrame &) = delete;
	WinFrame(WinFrame &&) = delete;
	~WinFrame();
	WinFrame &operator=(const WinFrame &) = delete;
	WinFrame &operator=(WinFrame &&) = delete;


	static void reg(RmlUISystem &rmlui);
	
protected:
	void onMounted() override;
	void onUpdate() override;
	void processDefaultAction(Rml::Event &event) override;
	void reload() override;

private:
	void initAfterConstruct();
	void bindEventHandlers();

	// Dragging
	bool isDragging_ = false;
	Rml::Vector2i mousePosWhenBeginDrag_;

	SimpleEventListenerManager eventListenerMan_{ *this };

	//TestListener testListener{ this };

};


#endif //NANOLIVELENS_WINFRAME_HXX
