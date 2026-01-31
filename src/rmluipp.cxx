#include "rmluipp.hxx"

#include <print>
#include <iostream>
#include <RmlUI/Core/Geometry.h>

ElementNotFoundErr::ElementNotFoundErr(std::string_view elementId)
    : std::runtime_error{
        std::format("Required element `{}' not found!", elementId)
    } { }

Rml::Element &requireElement(Rml::Element &parent, const std::string_view id) {
    const auto result = parent.GetElementById(std::string{ id });
    if (!result) {
        throw ElementNotFoundErr{ id };
    }

    return *result;
}

void printElementTree(const Rml::Element &parent) {
    std::cout << "BEGIN printElementTree\n";
    [](this const auto &self, const Rml::Element &child, int nest) -> void {
        for (int i = 0; i < nest; i++)
            std::cout << "  ";
        std::cout << "> ";

        std::print("tagName: {}, id: {}\n", child.GetTagName(), child.GetId());

        nest++;

        for (int i = 0; i < child.GetNumChildren(); i++)
            self(*child.GetChild(i), nest);
    }(parent, 0);
    std::cout << "END printElementTree" << std::endl;
}

Rml::Element *findParentOrSelfById(Rml::Element *child, const std::string_view id) {
	if (!child)
		return nullptr;

	if (child->GetId() == id)
		return child;

	auto parent = child->GetParentNode();
	if (!parent)
		return nullptr;

	if (parent->GetId() == id)
		return parent;
	else
		return findParentOrSelfById(parent, id);
}

Rml::Element *findChildOrSelfById(Rml::Element *parent, const std::string_view id) {
	if (!parent)
		return nullptr;

	if (parent->GetId() == id)
		return parent;

	for (int i = 0; i < parent->GetNumChildren(); i++) {
		auto child = parent->GetChild(i);
		if (child->GetId() == id)
			return child;
		else
			if (auto resultInChild = findChildOrSelfById(child, id))
				return resultInChild;
	}

	return nullptr;
}


SystemInterface_GLFW &getSysItfc() {
    return dynamic_cast<SystemInterface_GLFW &>(*Rml::GetSystemInterface());
}


SimpleEventListener::SimpleEventListener(std::function<void(Rml::Event &)> callback)
	: callback_{ std::move(callback) } { }

void SimpleEventListener::ProcessEvent(Rml::Event &event) {
	callback_(event);
}


SimpleEventListenerManager::SimpleEventListenerManager(Rml::Element &element)
    : element_{ &element } { }

void SimpleEventListenerManager::on(const std::string_view childElementId, const std::string_view event, std::function<void(Rml::Event &)> callback) {
    auto &childElement = EXCEPT(findChildOrSelfById(element_, childElementId), std::format("找不到子元素 '{}'", childElementId));
	eventListeners_.emplace_back(newBox(SimpleEventListener{ callback }));
    childElement.AddEventListener(std::string{ event }, &*eventListeners_.back());
}
