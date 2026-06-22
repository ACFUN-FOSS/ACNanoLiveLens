#include "rmluipp.hxx"

using namespace Essentials::Memory;

ElementNotFoundErr::ElementNotFoundErr(std::string_view elementId)
    : std::runtime_error{
        std::format("Required element `{}' not found!", elementId)
    } { }

Rml::Element &requireFactoryElement(Rml::Element &parent, const std::string_view id) {
    auto *result = parent.GetElementById(std::string{ id });
    assert(result && "Required factory element not found.");
    return *result;
}

Rml::Element &requireUserElement(Rml::Element &parent, const std::string_view id) {
    const auto result = parent.GetElementById(std::string{ id });
    if (!result) {
        throw ElementNotFoundErr{ id };
    }

    return *result;
}

Rml::Element &requireElement(Rml::Element &parent, const std::string_view id) {
    return requireUserElement(parent, id);
}

InvalidElementDynRefErr::InvalidElementDynRefErr(std::string_view elementQuery)
    : std::runtime_error{
        std::format("Cannot find element `{}'!", elementQuery)
    } { }

ElementDynRef::ElementDynRef(Rml::ElementDocument &document, std::string_view query)
	: document_{ &document }, query_{ query } { }

Rml::Element &ElementDynRef::resolve() {
	const auto result = document_->QuerySelector(query_);
	if (!result) {
		throw InvalidElementDynRefErr{ query_ };
	}
	return *result;
}

Rml::Element *ElementDynRef::operator->() {
	return &resolve();
}

void ElementDynRef::reBind(Rml::ElementDocument &document) {
	document_ = &document;
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

std::vector<Refw<Rml::Element>> getAllChildrenRecursively(Rml::Element &parent) {
	std::vector<Refw<Rml::Element>> children;
	
	auto collectChildren = [&](this const auto &self, Rml::Element &elem) -> void {
		for (int i = 0; i < elem.GetNumChildren(); i++) {
			auto *child = elem.GetChild(i);
			children.emplace_back(*child);
			self(*child);
		}
	};
	
	collectChildren(parent);
	return children;
}

SystemInterface_GLFW &getSysItfc() {
    return dynamic_cast<SystemInterface_GLFW &>(*Rml::GetSystemInterface());
}


SimpleEventListener::SimpleEventListener(std::function<void(Rml::Event &)> callback)
	: callback_{ std::move(callback) } { }

void SimpleEventListener::ProcessEvent(Rml::Event &event) {
	try {
		callback_(event);
	} catch (const std::exception &e) {
		std::println("Runtime error: {}", e.what());
	}
}

std::function<void(Rml::Event &)> &SimpleEventListener::getCallback() {
	return callback_;
}


SimpleEventListenerManager::SimpleEventListenerManager(Rml::Element &element LIFETIMEBOUND)
    : element_{ &element } { }


bool SimpleEventListenerManager::BindingRecord::operator==(const BindingRecord &other) const {
	return childElementId == other.childElementId && event == other.event;
}

size_t SimpleEventListenerManager::BindingRecord::Hasher::operator()(const BindingRecord &record) const {
	size_t h1 = std::hash<std::string_view>{ }(record.childElementId);
	size_t h2 = std::hash<std::string_view>{ }(record.event);
	return h1 ^ (h2 << 1);
}

SimpleEventListenerManager::~SimpleEventListenerManager() {
	clear();
}

void SimpleEventListenerManager::on(const std::string_view childElementId, const std::string_view event, std::function<void(Rml::Event &)> callback) {
	BindingRecord regRec {
		std::string{ childElementId },
		std::string{ event }
	};
	
	// 检查针对特定元素、特定事件的回调绑定是否已经存在
	auto callbackResistered = eventListeners_.find(regRec) != eventListeners_.end();

	if (callbackResistered)
		return;

    auto &childElement = EXCEPT(findChildOrSelfById(element_, childElementId), std::format("找不到子元素 '{}'", childElementId));

	auto it = eventListeners_.emplace(regRec, newBox(SimpleEventListener{ callback }));

    childElement.AddEventListener(std::string{ event }, &*it.first->second);
}

void SimpleEventListenerManager::clear() {
	for (auto &[regRec, listenerBox] : eventListeners_) {
		auto childElementPtr = findChildOrSelfById(element_, regRec.childElementId);

		if (!childElementPtr)
			continue;
		childElementPtr->RemoveEventListener(regRec.event, &*listenerBox);
	}
	eventListeners_.clear();
}

void SimpleEventListenerManager::reBind(Rml::Element &element) {
	for (auto &[regRec, listenerBox] : eventListeners_) {
		auto childElementPtr = findChildOrSelfById(element_, regRec.childElementId);

		if (!childElementPtr)
			continue;
		childElementPtr->RemoveEventListener(regRec.event, &*listenerBox);
	}
	element_ = &element;

	for (auto &[regRec, listenerBox] : eventListeners_) {
		auto childElementPtr = findChildOrSelfById(element_, regRec.childElementId);

		if (!childElementPtr)
			continue;
		childElementPtr->AddEventListener(regRec.event, &*listenerBox);
	}
}



TestListener::TestListener(Rml::Element *element)
	: element{ element } { }

void TestListener::ProcessEvent(Rml::Event &event) {
	if (event.GetId() == Rml::EventId::Mousedown) {
		std::cout << "TestListener. element clicked. addr = " << element << std::endl;
	}
}
