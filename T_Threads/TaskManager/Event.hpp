#pragma once
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
/// a listener for Event
class Listener {
public:
    virtual ~Listener() = default;
    virtual void on_event_triggered() = 0;
};

class Event {
public:
    Event() = default;  // Constructor
    Event& operator=(const Event& other) = delete; // Prevent copying
    Event(const Event& other) = delete; // Prevent copying
    virtual ~Event() = default;  // Virtual destructor
    // Subscribe a listener to the event
    void subscribe(std::shared_ptr<Listener> listener) {
        {
            std::lock_guard<std::mutex> lock(listenersMutex);
            listeners.emplace_back(std::move(listener));  // Add the listener to the list
        }
    }
    //unsubscribe a listener from the event
    void unsubscribe(const std::shared_ptr<Listener>& listener) {
        std::lock_guard<std::mutex> lock(listenersMutex);
        listeners.erase(
            std::remove_if(
                listeners.begin(), listeners.end(),
                [&](const std::shared_ptr<Listener>& l) {
                    return l == listener;
                }),
            listeners.end());
    }
    // Set a callback to be triggered when the event is notified
    void set_callback(std::function<void()> callback) {
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback_ = std::move(callback);
        }
    }
    // Notify all listeners and trigger the callback if defined
    void notify() {
        {
            std::lock_guard<std::mutex> lock(listenersMutex);
            for (const auto& listener : listeners) {
                listener.get()->on_event_triggered();
            }
            if (callback_) {
                callback_();
            }
        }
    }
    virtual std::string get_event_type() const = 0;

private:
    std::mutex listenersMutex;
    std::mutex callbackMutex;
    std::vector<std::shared_ptr<Listener>> listeners;  // Pool of listeners
    std::function<void()> callback_;  // A callback function to be executed on event trigger
};