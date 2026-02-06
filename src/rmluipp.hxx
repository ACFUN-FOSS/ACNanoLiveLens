#ifndef NANOLIVELENS_RMLUIPP_HXX
#define NANOLIVELENS_RMLUIPP_HXX
#include <RmlUi/Core/EventListener.h>

#include <concepts>
#include <string_view>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <functional>
#include <vector>
#include <unordered_map>

#include <EatiEssentials/memsafety.hxx>
#include <EatiEssentials/memory.hxx>
#include <RmlUi/Core/Element.h>
#include <RmlUi_Platform_GLFW.h>
#include <gsl/gsl>
#include <RmlUi/Core/Event.h>





class ElementNotFoundErr : public std::runtime_error
{
public:
    ElementNotFoundErr(std::string_view elementId);
};

Rml::Element &requireElement(Rml::Element &parent, const std::string_view id);

template <typename... Names>
requires (std::convertible_to<Names, std::string_view> && ...)
auto requireElements(Names&&... names) {
    return std::make_tuple(requireElement(std::forward<Names>(names))...);
}

void printElementTree(const Rml::Element &parent);

Rml::Element *findParentOrSelfById(Rml::Element *child, const std::string_view id);
Rml::Element *findChildOrSelfById(Rml::Element *parent, const std::string_view id);

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

// Lifetime depends on: element
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

    SimpleEventListenerManager(Rml::Element &element LIFETIMEBOUND);
	~SimpleEventListenerManager();

    void on(const std::string_view childElementId, const std::string_view event, std::function<void(Rml::Event &)> callback);


private:
    gsl::not_null<Rml::Element *> element_;

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
