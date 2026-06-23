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

std::string toDbgString(Rml::Vector2i pos) {
	return std::format("pos: {{x={}, y={}}}", pos.x, pos.y);
}

int randomInt(int min, int max) {
	static std::random_device r;
	static std::mt19937 mt{ r() };
	std::uniform_int_distribution<int> gen{ min, max };
	return gen(mt);
}

