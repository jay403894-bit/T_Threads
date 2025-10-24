#pragma once
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#pragma once
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
/// a listener_ for Event
class Listener {
public:
    virtual ~Listener() = default;
    virtual void onEventTriggered() = 0;
};

class Event {
public:
    Event() = default;  // Constructor
    Event& operator=(const Event& other) = delete; // Prevent copying
    Event(const Event& other) = delete; // Prevent copying
    virtual ~Event() = default;  // Virtual destructor
    // Subscribe a listener_ to the event
    void subscribe(std::shared_ptr<Listener> listener_) {
        {
            std::lock_guard<std::mutex> lock(listenersMutex);
            listeners.emplace_back(std::move(listener_));  // Add the listener_ to the list
        }
    }
    //unsubscribe a listener_ from the event
    void unsubscribe(const std::shared_ptr<Listener>& listener_) {
        std::lock_guard<std::mutex> lock(listenersMutex);
        listeners.erase(
            std::remove_if(
                listeners.begin(), listeners.end(),
                [&](const std::shared_ptr<Listener>& l) {
                    return l == listener_;
                }),
            listeners.end());
    }
    // Set a callback_ to be triggered when the event is notified
    void setCallback(std::function<void()> callback_) {
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback_ = std::move(callback_);
        }
    }
    // Notify all listeners and trigger the callback_ if defined
    void notify() {
        // Copy listeners and callback_ under their respective locks,
        // then invoke them outside the locks to avoid lock-order inversion.
        std::vector<std::shared_ptr<Listener>> listenersCopy;
        std::function<void()> callbackCopy;
        {
            std::lock_guard<std::mutex> lock(listenersMutex);
            listenersCopy = listeners;
        }
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callbackCopy = callback_;
        }

        // invoke outside locks
        for (const auto& listener_ : listenersCopy) {
            if (listener_) {
                try {
                    listener_->onEventTriggered();
                }
                catch (...) {
                    // swallow/log if logger available
                }
            }
        }
        if (callbackCopy) {
            try {
                callbackCopy();
            }
            catch (...) {
                // swallow/log
            }
        }
    }
    virtual std::string get_event_type() const = 0;

private:
    std::mutex listenersMutex;
    std::mutex callbackMutex;
    std::vector<std::shared_ptr<Listener>> listeners;  // Pool of listeners
    std::function<void()> callback_;  // A callback_ function to be executed on event trigger
};