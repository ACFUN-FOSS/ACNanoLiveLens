#ifndef NANOLIVELENS_SAFE_ELEMENT_INSTANCER_HXX
#define NANOLIVELENS_SAFE_ELEMENT_INSTANCER_HXX

// A thin wrapper around Rml::ElementInstancerGeneric that catches exceptions thrown
// by element constructors and returns a safe placeholder element instead. This
// prevents exceptions from propagating through RmlUI internals.

template <typename T>
struct SafeElementInstancer : Rml::ElementInstancerGeneric<T>
{
    Rml::ElementPtr InstanceElement(Rml::Element *parent, const Rml::String &tag, const Rml::XMLAttributes &attributes) override
    {
        try {
            //return Rml::ElementInstancerGeneric<T>::InstanceElement(parent, tag, attributes);
			return Rml::ElementPtr{ new T{ tag } };
        }
        catch (const std::exception &e) {
            std::println("SafeElementInstancer: element construction failed: {}", e.what());
			// TODO：可以考虑创建一个专门的 “construction failed” 元素，显示一些错误信息，而不是一个普通的 div。
			return Rml::ElementPtr{ new Rml::Element{ "div" } };
        }
    }

    void ReleaseElement(Rml::Element* element) override
    {
		delete element;
    }
};

#endif // NANOLIVELENS_SAFE_ELEMENT_INSTANCER_HXX
