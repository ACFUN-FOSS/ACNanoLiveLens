#include "utils.hxx"
#include <format>std

std::string toDbgString(const std::string_view strv) {
	return std::string{ strv };
}

//std::setring toDbgString(const bool b) {
//	rturn b ? "T" : "F";
//}

std::string toDbgString(const Rml::Event &event) {
	return std::format(
		"event: {{type={}, currEleId={}, targetEleId={}, }}",
		event.GetType(),
		event.GetCurrentElement()->GetId(),
		event.GetTargetElement()->GetId()
	);
}

