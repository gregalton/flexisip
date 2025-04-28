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

    bool subscribe(const std::string& topic, std::weak_ptr<ContactRegisteredListener>&& listener) override {
        // Add the listener to the map
        mContactListenersMap[topic] = std::move(listener);
        
        // Subscribe to the topic in Redis
        if (mSubscribeContext) {
            redisAsyncCommand(mSubscribeContext, sPublishCallback, nullptr, "SUBSCRIBE %s", topic.c_str());
            return true;
        }
        return false;
    }

    void unsubscribe(const std::string& topic, const std::shared_ptr<ContactRegisteredListener>& listener) override {
        // Remove the listener from the map
        mContactListenersMap.erase(topic);
        
        // Unsubscribe from the topic in Redis
        if (mSubscribeContext) {
            redisAsyncCommand(mSubscribeContext, nullptr, nullptr, "UNSUBSCRIBE %s", topic.c_str());
        }
    }

    void onContactRegistered(const std::shared_ptr<Record>& r, const std::string& uid) override {
        LOGD("Contact registered for topic = %s, uid = %s", r->getKey().c_str(), uid.c_str());
        
        // Notify all listeners for this topic
        auto range = mContactListenersMap.equal_range(r->getKey());
        for (auto it = range.first; it != range.second;) {
            if (auto strongPtr = it->second.lock()) {
                strongPtr->onContactRegistered(r, uid);
                ++it;
            } else {
                // Remove expired listener
                it = mContactListenersMap.erase(it);
            }
        }
    }

protected:
    void doBind(const sofiasip::MsgSip& sip,
                const BindingParameters& parameters,
                const std::shared_ptr<ContactUpdateListener>& listener) override {
        LOGD("Binding contact in Redis");
        
        // Create a new record or get existing one
        auto record = std::make_shared<Record>();
        record->setKey(Record::defineKeyFromUrl(sip.getSip()->sip_from->a_url));
        
        // Add contact to record
        auto contact = std::make_shared<ExtendedContact>(sip.getSip()->sip_contact, parameters);
        record->addContact(contact);
        
        // Store in Redis
        try {
            mRedisClient->set(record->getKey(), record->serialize());
            listener->onRecordFound(record);
        } catch (const std::exception& e) {
            SLOGE << "Failed to bind contact in Redis: " << e.what();
            listener->onError();
        }
    }

    void doClear(const sofiasip::MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) override {
        LOGD("Clearing record from Redis");
        
        try {
            auto key = Record::defineKeyFromUrl(sip.getSip()->sip_from->a_url);
            mRedisClient->del(key);
            listener->onRecordFound(nullptr);
        } catch (const std::exception& e) {
            SLOGE << "Failed to clear record from Redis: " << e.what();
            listener->onError();
        }
    }

    void doFetch(const sofiasip::MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) override {
        LOGD("Fetching record from Redis");
        
        try {
            auto key = Record::defineKeyFromUrl(sip.getSip()->sip_from->a_url);
            auto value = mRedisClient->get(key);
            
            if (value.empty()) {
                listener->onRecordFound(nullptr);
                return;
            }
            
            auto record = std::make_shared<Record>(key);
            record->deserialize(value);
            listener->onRecordFound(record);
        } catch (const std::exception& e) {
            SLOGE << "Failed to fetch record from Redis: " << e.what();
            listener->onError();
        }
    }

    void doFetchInstance(const sofiasip::MsgSip& sip, const std::string& uniqueId,
                        const std::shared_ptr<ContactUpdateListener>& listener) override {
        LOGD("Fetching record instance from Redis");
        
        try {
            auto key = Record::defineKeyFromUrl(sip.getSip()->sip_from->a_url);
            auto value = mRedisClient->get(key);
            
            if (value.empty()) {
                listener->onRecordFound(nullptr);
                return;
            }
            
            auto record = std::make_shared<Record>(key);
            record->deserialize(value);
            
            // Find the specific instance
            auto contacts = record->getExtendedContacts();
            auto it = std::find_if(contacts.begin(), contacts.end(),
                [&uniqueId](const auto& contact) {
                    return contact->mUniqueId == uniqueId;
                });
                
            if (it != contacts.end()) {
                auto instanceRecord = std::make_shared<Record>(key);
                instanceRecord->insert(*it);
                listener->onRecordFound(instanceRecord);
            } else {
                listener->onRecordFound(nullptr);
            }
        } catch (const std::exception& e) {
            SLOGE << "Failed to fetch record instance from Redis: " << e.what();
            listener->onError();
        }
    }

    void doMigration() override {
        // Implementation of doMigration
        // This is a no-op for now as we don't have any migration logic
    }

private:
    std::shared_ptr<RedisClient> mRedisClient;
    std::unordered_multimap<std::string, std::weak_ptr<ContactRegisteredListener>> mContactListenersMap;
    redisAsyncContext* mSubscribeContext;
}; 