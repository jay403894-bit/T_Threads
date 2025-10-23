#pragma once
#include <string>
#include <iomanip>
#include <sstream>
#include <random>
#include <mutex>
//UniqueID generates a UUID tag for an object
class UniqueID {
public:
    //constructor
    UniqueID();
    //deleted copy constructor 
    UniqueID(const UniqueID& in) = delete;
    //destructor 
    virtual ~UniqueID() = default;
    //return the id as type uuid
    virtual std::string GetID();
    //equality/inequality check on ids
    bool operator==(const UniqueID& other) const;
    bool operator!=(const UniqueID& other) const;
protected:
    // the object id
    std::string objectID;  // UUID object as ID
private:
    // set the id of the object
    virtual void SetID(const std::string& id);
    // generate id
    std::string GenerateUUID();
};