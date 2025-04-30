#pragma once

#include "registrar-interfaces.hh"
#include "registrardb-redis.hh"
#include "redis-async-session.hh"

namespace flexisip {

class RedisExpirationHandler : public IExpirationHandler {
public:
    RedisExpirationHandler(std::shared_ptr<RedisAsyncSession> redisClient)
        : mRedisClient(std::move(redisClient)) {}

    std::vector<std::shared_ptr<ExtendedContact>> fetchExpiringContacts(
        const std::chrono::system_clock::time_point& time,
        const std::chrono::seconds& threshold) override {
        // Implementation will be moved from RegistrarDbRedisAsync
        return {};
    }

    void fetchExpiringContacts(time_t startTimestamp, float threshold,
                             std::function<void(std::vector<ExtendedContact>&&)>&& callback) override {
        // Implementation will be moved from RegistrarDbRedisAsync
    }

private:
    std::shared_ptr<RedisAsyncSession> mRedisClient;
};

class RedisActivityTracker : public IActivityTracker {
public:
    RedisActivityTracker(std::shared_ptr<RedisAsyncSession> redisClient)
        : mRedisClient(std::move(redisClient)) {}

    void updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) override {
        // Implementation will be moved from RegistrarDbRedisAsync
    }

private:
    std::shared_ptr<RedisAsyncSession> mRedisClient;
};

class RedisContactManager : public IContactManager {
public:
    RedisContactManager(std::shared_ptr<RedisAsyncSession> redisClient)
        : mRedisClient(std::move(redisClient)) {}

    void bind(const MsgSip& sip, const BindingParameters& parameters, 
             const std::shared_ptr<ContactUpdateListener>& listener) override {
        // Implementation will be moved from RegistrarDbRedisAsync
    }

    void clear(const MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) override {
        // Implementation will be moved from RegistrarDbRedisAsync
    }

    void fetch(const SipUri& url, const std::shared_ptr<ContactUpdateListener>& listener) override {
        // Implementation will be moved from RegistrarDbRedisAsync
    }

    void fetchInstance(const SipUri& url, const std::string& uniqueId,
                      const std::shared_ptr<ContactUpdateListener>& listener) override {
        // Implementation will be moved from RegistrarDbRedisAsync
    }

private:
    std::shared_ptr<RedisAsyncSession> mRedisClient;
};

class RedisPubSub : public IPubSub {
public:
    RedisPubSub(std::shared_ptr<RedisAsyncSession> redisClient)
        : mRedisClient(std::move(redisClient)) {}

    void publish(const std::string& topic, const std::string& uid) override {
        // Implementation will be moved from RegistrarDbRedisAsync
    }

    bool subscribe(const std::string& topic, 
                  std::weak_ptr<ContactRegisteredListener>&& listener) override {
        // Implementation will be moved from RegistrarDbRedisAsync
        return false;
    }

    void unsubscribe(const std::string& topic, 
                    const std::shared_ptr<ContactRegisteredListener>& listener) override {
        // Implementation will be moved from RegistrarDbRedisAsync
    }

private:
    std::shared_ptr<RedisAsyncSession> mRedisClient;
};

} // namespace flexisip 