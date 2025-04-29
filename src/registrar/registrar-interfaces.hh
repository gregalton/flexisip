#pragma once

#include <memory>
#include <vector>
#include <chrono>
#include <functional>

namespace flexisip {

class ExtendedContact;
class ContactUpdateListener;
class ContactRegisteredListener;
class BindingParameters;
class SipUri;

// Interface for expiration handling
class IExpirationHandler {
public:
    virtual std::vector<std::shared_ptr<ExtendedContact>> fetchExpiringContacts(
        const std::chrono::system_clock::time_point& time,
        const std::chrono::seconds& threshold) = 0;
    virtual void fetchExpiringContacts(time_t startTimestamp, float threshold,
                                     std::function<void(std::vector<ExtendedContact>&&)>&& callback) const = 0;
    virtual ~IExpirationHandler() = default;
};

// Interface for activity tracking
class IActivityTracker {
public:
    virtual void updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) = 0;
    virtual ~IActivityTracker() = default;
};

// Interface for contact management
class IContactManager {
public:
    virtual void bind(const MsgSip& sip, const BindingParameters& parameters, 
                     const std::shared_ptr<ContactUpdateListener>& listener) = 0;
    virtual void clear(const MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) = 0;
    virtual void fetch(const SipUri& url, const std::shared_ptr<ContactUpdateListener>& listener) = 0;
    virtual void fetchInstance(const SipUri& url, const std::string& uniqueId,
                             const std::shared_ptr<ContactUpdateListener>& listener) = 0;
    virtual ~IContactManager() = default;
};

// Interface for Redis operations
class IRedisOperations {
public:
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual void setWritable(bool value) = 0;
    virtual ~IRedisOperations() = default;
};

// Interface for Pub/Sub functionality
class IPubSub {
public:
    virtual void publish(const std::string& topic, const std::string& uid) = 0;
    virtual bool subscribe(const std::string& topic, 
                         std::weak_ptr<ContactRegisteredListener>&& listener) = 0;
    virtual void unsubscribe(const std::string& topic, 
                           const std::shared_ptr<ContactRegisteredListener>& listener) = 0;
    virtual ~IPubSub() = default;
};

} // namespace flexisip 