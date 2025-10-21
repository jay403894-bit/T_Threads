#include "Object.h"
Object::Object() { SetID(GenerateUUID()); }
//return the id
std::string Object::GetID() const {
    return objectID;
}
//equality operator
bool Object::operator==(const Object& other) const {
    return objectID == other.objectID;
}
//inequality operator
bool Object::operator!=(const Object& other) const {
    return objectID != other.objectID;
}
//set the id
void Object::SetID(const std::string& id) { objectID = id; }
//generate uuid
std::string Object::GenerateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15); // Hex digits (0 to F)

    std::stringstream uuid;
    uuid << std::hex << std::setfill('0');

    // First group (8 hex digits)
    for (int i = 0; i < 8; ++i) {
        uuid << std::setw(1) << dis(gen);
    }
    uuid << "-";

    // Second group (4 hex digits)
    for (int i = 0; i < 4; ++i) {
        uuid << std::setw(1) << dis(gen);
    }
    uuid << "-";

    // Third group: Version 4 UUID (4 as the first hex digit)
    uuid << "4"; // Version 4
    for (int i = 0; i < 3; ++i) {
        uuid << std::setw(1) << dis(gen);
    }
    uuid << "-";

    // Fourth group: Variant (first hex digit is 8, 9, A, or B)
    uuid << std::hex << std::setw(1) << (dis(gen) & 0x3 | 0x8); // Ensures the first hex digit is in the range [8, B]
    for (int i = 0; i < 3; ++i) {
        uuid << std::setw(1) << dis(gen);
    }
    uuid << "-";

    // Fifth group (12 hex digits)
    for (int i = 0; i < 12; ++i) {
        uuid << std::setw(1) << dis(gen);
    }

    return uuid.str();
}