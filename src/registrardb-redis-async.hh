class RegistrarDbRedisAsync : public RegistrarDb {
public:
    RegistrarDbRedisAsync(Agent* ag, const std::shared_ptr<RedisClient>& redisClient);
    ~RegistrarDbRedisAsync() override = default;

    std::vector<std::shared_ptr<ExtendedContact>> fetchExpiringContacts(
        const std::chrono::system_clock::time_point& now,
        const std::chrono::seconds& threshold) override {
        std::vector<std::shared_ptr<ExtendedContact>> expiringContacts;
        
        // Get all keys matching the pattern for contacts
        auto keys = mRedisClient->keys("fs:*");
        
        for (const auto& key : keys) {
            // Get the contact data
            auto contactData = mRedisClient->hgetall(key);
            
            // Parse each contact in the hash
            for (size_t i = 0; i < contactData.size(); i += 2) {
                const auto& field = contactData[i];
                const auto& value = contactData[i + 1];
                
                try {
                    // Create ExtendedContact from the serialized data
                    auto contact = std::make_shared<ExtendedContact>(field, value);
                    
                    // Check if the contact is expiring within the threshold
                    auto expirationTime = contact->getExpireTime();
                    auto timeUntilExpiration = expirationTime - now;
                    
                    if (timeUntilExpiration <= threshold) {
                        expiringContacts.push_back(contact);
                    }
                } catch (const std::exception& e) {
                    SLOGE << "Failed to parse contact data for field " << field << ": " << e.what();
                    continue;
                }
            }
        }
        
        return expiringContacts;
    }

    void fetchExpiringContacts(time_t startTimestamp,
                              float threshold,
                              std::function<void(std::vector<ExtendedContact>&&)>&& callback) const override {
        std::vector<ExtendedContact> expiringContacts;
        
        // Get all keys matching the pattern for contacts
        auto keys = mRedisClient->keys("fs:*");
        
        for (const auto& key : keys) {
            // Get the contact data
            auto contactData = mRedisClient->hgetall(key);
            
            // Parse each contact in the hash
            for (size_t i = 0; i < contactData.size(); i += 2) {
                const auto& field = contactData[i];
                const auto& value = contactData[i + 1];
                
                try {
                    // Create ExtendedContact from the serialized data
                    ExtendedContact contact(field, value);
                    
                    // Check if the contact is expiring within the threshold
                    auto expirationTime = contact.getExpireTime();
                    auto timeUntilExpiration = std::chrono::duration_cast<std::chrono::seconds>(
                        expirationTime - std::chrono::system_clock::from_time_t(startTimestamp));
                    
                    if (timeUntilExpiration.count() <= threshold && timeUntilExpiration.count() > 0) {
                        expiringContacts.push_back(std::move(contact));
                    }
                } catch (const std::exception& e) {
                    SLOGE << "Failed to parse contact data for field " << field << ": " << e.what();
                    continue;
                }
            }
        }
        
        callback(std::move(expiringContacts));
    }

    void updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) override {
        contact->updateLastActivityTime();
        auto record = std::make_shared<Record>(contact->getKey());
        record->insert(contact);
        
        // Update the contact in Redis
        auto redis = mRedisClient->getRedis();
        redis.hset(contact->getKey().str(), "last_activity", 
                   std::to_string(std::chrono::system_clock::to_time_t(contact->getLastActivityTime())));
    }

    void publish(const std::string& topic, const std::string& uid) override {
        // Publish the UID to the Redis channel for the topic
        mRedisClient->publish("registrar:" + topic, uid);
    }

protected:
    void doBind(const sofiasip::MsgSip& sip,
                const BindingParameters& parameters,
                const std::shared_ptr<ContactUpdateListener>& listener) override {
        // Implementation of doBind
        auto redis = mRedisClient->getRedis();
        auto key = sip.getSip()->sip_from->a_url->url_user;
        
        // Create or update contact
        auto contact = std::make_shared<ExtendedContact>(sip, parameters);
        auto record = std::make_shared<Record>(key);
        record->insert(contact);
        
        // Store in Redis
        redis.hset(key, contact->getUniqueId(), contact->serialize());
        
        // Notify listener
        if (listener) {
            listener->onRecordFound(record);
        }
    }

    void doClear(const sofiasip::MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) override {
        // Implementation of doClear
        auto redis = mRedisClient->getRedis();
        auto key = sip.getSip()->sip_from->a_url->url_user;
        
        // Delete from Redis
        redis.del(key);
        
        // Notify listener
        if (listener) {
            listener->onRecordFound(std::make_shared<Record>(key));
        }
    }

    void doFetch(const SipUri& url, const std::shared_ptr<ContactUpdateListener>& listener) override {
        // Implementation of doFetch
        auto redis = mRedisClient->getRedis();
        auto key = url.getUser();
        
        // Get all contacts for this key
        auto contactData = redis.hgetall(key);
        auto record = std::make_shared<Record>(key);
        
        // Parse contacts
        for (size_t i = 0; i < contactData.size(); i += 2) {
            const auto& field = contactData[i];
            const auto& value = contactData[i + 1];
            
            try {
                auto contact = std::make_shared<ExtendedContact>(field, value);
                record->insert(contact);
            } catch (const std::exception& e) {
                SLOGE << "Failed to parse contact data for field " << field << ": " << e.what();
                continue;
            }
        }
        
        // Notify listener
        if (listener) {
            listener->onRecordFound(record);
        }
    }

    void doFetchInstance(const SipUri& url,
                         const std::string& uniqueId,
                         const std::shared_ptr<ContactUpdateListener>& listener) override {
        // Implementation of doFetchInstance
        auto redis = mRedisClient->getRedis();
        auto key = url.getUser();
        
        // Get specific contact
        auto contactData = redis.hget(key, uniqueId);
        auto record = std::make_shared<Record>(key);
        
        if (!contactData.empty()) {
            try {
                auto contact = std::make_shared<ExtendedContact>(uniqueId, contactData);
                record->insert(contact);
            } catch (const std::exception& e) {
                SLOGE << "Failed to parse contact data for uniqueId " << uniqueId << ": " << e.what();
            }
        }
        
        // Notify listener
        if (listener) {
            listener->onRecordFound(record);
        }
    }

    void doMigration() override {
        // Implementation of doMigration
        // This is a no-op for now as we don't have any migration logic
    }

private:
    std::shared_ptr<RedisClient> mRedisClient;
}; 