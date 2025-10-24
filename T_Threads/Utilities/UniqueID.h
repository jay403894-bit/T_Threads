#pragma once
#include <string>
#include <iomanip>
#include <sstream>
#include <random>
#include <mutex>

class UniqueID {
public:
    UniqueID();
    UniqueID(const UniqueID& in) = delete;
    virtual ~UniqueID() = default;
    virtual std::string getId();
    bool operator==(const UniqueID& other) const;
    bool operator!=(const UniqueID& other) const;
protected:
    std::string object_id_;  
private:
    virtual void setId(const std::string& id_);
    std::string GenerateUUID();
};