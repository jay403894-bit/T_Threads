#pragma once
#include <string>
#include <iomanip>
#include <sstream>
#include <random>
#include <mutex>
//Object generates a UUID tag for an object
class Object {
public:
    //constructor
    Object();
    //deleted copy constructor 
    Object(const Object& in) = delete;
    //destructor 
    virtual ~Object() = default;
    //return the id as type uuid
    virtual std::string GetID();
    //equality/inequality check on ids
    bool operator==(const Object& other) const;
    bool operator!=(const Object& other) const;
protected:
    // the object id
    std::string objectID;  // UUID object as ID
private:
    // set the id of the object
    virtual void SetID(const std::string& id);
    // generate id
    std::string GenerateUUID();
};