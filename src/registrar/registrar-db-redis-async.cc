#include "registrar-db-redis-async.hh"

#include <chrono>
#include <memory>
#include <vector>

#include "flexisip/logmanager.hh"
#include "registrar/record.hh"
#include "registrar/extended-contact.hh"

namespace flexisip {

std::vector<std::shared_ptr<ExtendedContact>> RegistrarDbRedisAsync::fetchExpiringContacts(
    const std::chrono::system_clock::time_point& now,
    const std::chrono::seconds& threshold) {
    std::vector<std::shared_ptr<ExtendedContact>> result;
    auto redis = mRedisClient->getRedis();
    
    // Get all keys matching the pattern
    auto keys = redis.keys("fs:*");
    
    for (const auto& key : keys) {
        auto record = std::make_shared<Record>(key);
        auto contacts = record->getExtendedContacts();
        
        for (const auto& contact : contacts) {
            auto expiresAt = contact->getRegisterTime() + contact->getSipExpires().count();
            auto timeUntilExpiration = std::chrono::seconds(expiresAt) - 
                                     std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
            
            if (timeUntilExpiration <= threshold) {
                result.push_back(contact);
            }
        }
    }
    
    return result;
}

void RegistrarDbRedisAsync::updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) {
    contact->updateLastActivityTime();
    auto record = std::make_shared<Record>(contact->getKey());
    record->insert(contact);
    
    // Update the contact in Redis
    auto redis = mRedisClient->getRedis();
    redis.hset(contact->getKey().str(), "last_activity", 
               std::to_string(std::chrono::system_clock::to_time_t(contact->getLastActivityTime())));
}

} // namespace flexisip 