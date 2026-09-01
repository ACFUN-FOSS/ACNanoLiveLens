#ifndef NANOLIVELENS_RMLUIPP_HXX
#define NANOLIVELENS_RMLUIPP_HXX
#include "RmlUi_Platform_GLFW.h"
#include "RmlUIWin/window_manager.hxx"

class ElementNotFoundErr : public std::runtime_error
{
public:
    ElementNotFoundErr(std::string_view elementId);
};

Rml::Element &requireFactoryElement(Rml::Element &parent, const std::string_view id);
Rml::Element &requireUserElement(Rml::Element &parent, const std::string_view id);
Rml::Element &requireElement(Rml::Element &parent, const std::string_view id);

template <typename... Names>
requires (std::convertible_to<Names, std::string_view> && ...)
auto requireElements(Names&&... names) {
    return std::make_tuple(requireElement(std::forward<Names>(names))...);
}

class InvalidElementDynRefErr : public std::runtime_error
{
public:
    InvalidElementDynRefErr(std::string_view elementQuery);
};

class ElementDynRef
{
public:
    ElementDynRef(RmlUIWin::UiWin &window, std::string_view query);
	Rml::Element &resolve();
	Rml::Element *operator->();
private:
	gsl::not_null<RmlUIWin::UiWin *> window_;
	std::string query_;
};


void printElementTree(const Rml::Element &parent);

Rml::Element *findParentOrSelfById(Rml::Element *child, const std::string_view id);
Rml::Element *findChildOrSelfById(Rml::Element *parent, const std::string_view id);
std::vector<ESSM::Refw<Rml::Element>> getAllChildrenRecursively(Rml::Element &parent);

SystemInterface_GLFW &getSysItfc();


class SimpleEventListener : public Rml::EventListener
{
public:
	SimpleEventListener(std::function<void(Rml::Event &)> callback);

	void ProcessEvent(Rml::Event &event) override;

	std::function<void(Rml::Event &)> &getCallback();

private:
    std::function<void(Rml::Event &)> callback_;
};

// Lifetime depends on: window
class SimpleEventListenerManager
{
public:

	struct BindingRecord
	{
		std::string childElementId;
		std::string event;

		bool operator==(const BindingRecord &other) const;

		struct Hasher
		{
			size_t operator()(const BindingRecord &record) const;
		};
	};

    SimpleEventListenerManager(RmlUIWin::UiWin &window LIFETIMEBOUND);
    SimpleEventListenerManager(Rml::Element &element LIFETIMEBOUND);
	SimpleEventListenerManager(const SimpleEventListenerManager &) = delete;
	SimpleEventListenerManager(SimpleEventListenerManager &&)  noexcept = default;
	SimpleEventListenerManager &operator=(const SimpleEventListenerManager &) = delete;
	SimpleEventListenerManager &operator=(SimpleEventListenerManager &&) = delete;
	~SimpleEventListenerManager();

	void on(const std::string_view childElementId, const std::string_view event, std::function<void(Rml::Event &)> callback);
	void clear();

private:
    void bindToCurrentDocument();
    void unbindFromCurrentDocument();

    RmlUIWin::UiWin *window_ = nullptr;
	RmlUIWin::DocumentChangedObserverToken documentObserver_;
	Rml::Element *element_ = nullptr;

	std::unordered_map<BindingRecord, ESSM::Box<SimpleEventListener>, BindingRecord::Hasher> eventListeners_;

};


class TestListener : public Rml::EventListener
{
	Rml::Element *element = nullptr;

public:
	void ProcessEvent(Rml::Event &event) override;

	TestListener(Rml::Element *element);
};


#endif //NANOLIVELENS_RMLUIPP_HXX
