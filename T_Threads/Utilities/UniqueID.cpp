#include "UniqueID.h"
UniqueID::UniqueID() { SetID(GenerateUUID()); }
//return the id
std::string UniqueID::GetID() {
    return objectID;
}
//equality operator
bool UniqueID::operator==(const UniqueID& other) const {
    return objectID == other.objectID;
}
//inequality operator
bool UniqueID::operator!=(const UniqueID& other) const
{
    return objectID != other.objectID;
}
//set the id
void UniqueID::SetID(const std::string& id) { objectID = id; }

std::string UniqueID::GenerateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 15); // Hex digits 0-F

    auto hexDigit = [&]() -> char {
        int val = dis(gen);
        return val < 10 ? '0' + val : 'a' + (val - 10);
        };

    std::stringstream uuid;

    // 8-4-4-4-12 format
    for (int i = 0; i < 8; ++i) uuid << hexDigit();
    uuid << '-';
    for (int i = 0; i < 4; ++i) uuid << hexDigit();
    uuid << '-';

    // Version 4: first digit is '4'
    uuid << '4';
    for (int i = 0; i < 3; ++i) uuid << hexDigit();
    uuid << '-';

    // Variant: first digit 8-B
    int variant = (dis(gen) & 0x3) | 0x8;
    uuid << std::hex << variant;
    for (int i = 0; i < 3; ++i) uuid << hexDigit();
    uuid << '-';

    for (int i = 0; i < 12; ++i) uuid << hexDigit();

    return uuid.str();
}