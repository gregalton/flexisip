class RegistrarDbRedisAsync : public RegistrarDb {
public:
    // ... existing code ...

    std::vector<std::shared_ptr<ExtendedContact>> fetchExpiringContacts(
        const std::chrono::system_clock::time_point& time,
        const std::chrono::seconds& threshold) override {
        std::vector<std::shared_ptr<ExtendedContact>> expiringContacts;
        
        // Convert time_point to seconds since epoch
        auto time_seconds = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
        
        // Get all keys matching the pattern for contacts
        auto keys = mRedisClient->keys("contact:*");
        
        for (const auto& key : keys) {
            // Get contact data from Redis
            auto contactData = mRedisClient->hgetall(key);
            if (contactData.empty()) continue;
            
            // Parse expiration time from contact data
            auto it = contactData.find("expires");
            if (it == contactData.end()) continue;
            
            auto expires = std::stoll(it->second);
            auto time_until_expiry = expires - time_seconds;
            
            // If contact is expiring within the threshold, add it to the list
            if (time_until_expiry <= threshold.count() && time_until_expiry > 0) {
                auto contact = std::make_shared<ExtendedContact>();
                // TODO: Populate contact with data from Redis
                expiringContacts.push_back(contact);
            }
        }
        
        return expiringContacts;
    }

    void updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) override {
        if (!contact) return;
        
        // Update last activity time in Redis
        auto key = "contact:" + contact->mKey;
        auto now = std::chrono::system_clock::now();
        auto now_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        
        mRedisClient->hset(key, "last_activity", std::to_string(now_seconds));
    }

    // ... rest of existing code ...
}; 