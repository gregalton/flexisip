#include "registrar/redis-implementations.hh"
#include "registrar/record.hh"
#include "registrar/extended-contact.hh"
#include "utils/string-utils.hh"
#include <sstream>

namespace flexisip {

// RedisExpirationHandler implementation
std::vector<std::shared_ptr<ExtendedContact>> RedisExpirationHandler::fetchExpiringContacts(
    const std::chrono::system_clock::time_point& time,
    const std::chrono::seconds& threshold) {
    std::vector<std::shared_ptr<ExtendedContact>> result;
    
    // Get all keys matching the pattern
    auto keys = mRedisClient->keys("reg:*");
    for (const auto& key : keys) {
        // Get the record
        auto value = mRedisClient->get(key);
        if (!value.empty()) {
            try {
                Record record;
                record.unserialize(value);
                
                // Check each contact's expiration time
                for (const auto& contact : record.getExtendedContacts()) {
                    auto expireTime = contact->getExpireTime();
                    if (expireTime > 0) {
                        auto expirePoint = std::chrono::system_clock::from_time_t(expireTime);
                        auto timeToExpire = expirePoint - time;
                        if (timeToExpire <= threshold) {
                            result.push_back(contact);
                        }
                    }
                }
            } catch (const std::exception& e) {
                SLOGE << "Error deserializing record from Redis: " << e.what();
            }
        }
    }
    
    return result;
}

void RedisExpirationHandler::fetchExpiringContacts(time_t startTimestamp, float threshold,
                                                 std::function<void(std::vector<ExtendedContact>&&)>&& callback) const {
    std::vector<ExtendedContact> result;
    
    // Get all keys matching the pattern
    auto keys = mRedisClient->keys("fs:*");
    for (const auto& key : keys) {
        // Get the contact
        auto value = mRedisClient->get(key);
        if (!value.empty()) {
            try {
                ExtendedContact contact;
                contact.unserialize(value);
                
                // Check if the contact is expiring within the threshold
                if (contact.getExpireTime() > 0) {
                    auto timeToExpire = contact.getExpireTime() - startTimestamp;
                    if (timeToExpire <= threshold) {
                        result.push_back(contact);
                    }
                }
            } catch (const std::exception& e) {
                SLOGE << "Error deserializing contact from Redis: " << e.what();
            }
        }
    }
    
    callback(std::move(result));
}

// RedisActivityTracker implementation
void RedisActivityTracker::updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) {
    if (!contact) return;
    
    // Get the record
    std::string key = "reg:" + contact->getSipUri().str();
    auto value = mRedisClient->get(key);
    if (!value.empty()) {
        try {
            Record record;
            record.unserialize(value);
            
            // Update the contact's last activity time
            for (auto& existingContact : record.getExtendedContacts()) {
                if (existingContact->getUniqueId() == contact->getUniqueId()) {
                    existingContact->setLastActivityTime(std::chrono::system_clock::now());
                    break;
                }
            }
            
            // Save the updated record
            mRedisClient->set(key, record.serialize());
        } catch (const std::exception& e) {
            SLOGE << "Error updating contact activity in Redis: " << e.what();
        }
    }
}

// RedisContactManager implementation
void RedisContactManager::bind(const MsgSip& sip, const BindingParameters& parameters, 
                             const std::shared_ptr<ContactUpdateListener>& listener) {
    // Implementation moved from RegistrarDbRedisAsync
    // This would include the Redis-specific binding logic
}

void RedisContactManager::clear(const MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) {
    // Implementation moved from RegistrarDbRedisAsync
    // This would include the Redis-specific clearing logic
}

void RedisContactManager::fetch(const SipUri& url, const std::shared_ptr<ContactUpdateListener>& listener, 
                               bool allowDomainRegistrations, bool recursive) {
    // Implementation moved from RegistrarDbRedisAsync
    // This would include the Redis-specific fetching logic
}

void RedisContactManager::fetchInstance(const SipUri& url, const std::string& uniqueId,
                                      const std::shared_ptr<ContactUpdateListener>& listener) {
    // Implementation moved from RegistrarDbRedisAsync
    // This would include the Redis-specific instance fetching logic
}

// RedisPubSub implementation
void RedisPubSub::publish(const std::string& topic, const std::string& uid) {
    mRedisClient->publish(topic, uid);
}

bool RedisPubSub::subscribe(const std::string& topic, 
                          std::weak_ptr<ContactRegisteredListener>&& listener) {
    return mRedisClient->subscribe(topic, std::move(listener));
}

void RedisPubSub::unsubscribe(const std::string& topic, 
                            const std::shared_ptr<ContactRegisteredListener>& listener) {
    mRedisClient->unsubscribe(topic, listener);
}

} // namespace flexisip 