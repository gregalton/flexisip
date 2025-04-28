class RegistrarDbRedisAsync : public RegistrarDb {
public:
    RegistrarDbRedisAsync(Agent* ag, const std::shared_ptr<RedisClient>& redisClient);
    ~RegistrarDbRedisAsync() override = default;

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
                const std::shared_ptr<ContactUpdateListener>& listener) override;
    void doClear(const sofiasip::MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) override;
    void doFetch(const SipUri& url, const std::shared_ptr<ContactUpdateListener>& listener) override;
    void doFetchInstance(const SipUri& url,
                         const std::string& uniqueId,
                         const std::shared_ptr<ContactUpdateListener>& listener) override;
    void doMigration() override;

private:
    std::shared_ptr<RedisClient> mRedisClient;
}; 