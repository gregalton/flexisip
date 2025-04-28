class RegistrarDbRedisAsync : public RegistrarDb {
public:
    RegistrarDbRedisAsync(Agent* ag, const std::shared_ptr<RedisClient>& redisClient);
    ~RegistrarDbRedisAsync() override = default;

    void fetchExpiringContacts(time_t startTimestamp,
                              float threshold,
                              std::function<void(std::vector<ExtendedContact>&&)>&& callback) const override;

    std::vector<std::shared_ptr<ExtendedContact>> fetchExpiringContacts(
        const std::chrono::system_clock::time_point& time,
        const std::chrono::seconds& threshold) override;

    void updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) override;

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
                const std::shared_ptr<ContactUpdateListener>& listener) override;
    void doClear(const sofiasip::MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) override;
    void doFetch(const SipUri& url, const std::shared_ptr<ContactUpdateListener>& listener) override;
    void doFetchInstance(const SipUri& url,
                         const std::string& uniqueId,
                         const std::shared_ptr<ContactUpdateListener>& listener) override;
    void doMigration() override;

private:
    std::shared_ptr<RedisClient> mRedisClient;
    std::unordered_multimap<std::string, std::weak_ptr<ContactRegisteredListener>> mContactListenersMap;
    redisAsyncContext* mSubscribeContext;
}; 