#pragma once

#include "registrar-db.hh"
#include "redis-client.hh"

namespace flexisip {

class RegistrarDbRedisAsync : public RegistrarDb {
public:
    RegistrarDbRedisAsync(Agent* ag, const std::shared_ptr<RedisClient>& redisClient);
    ~RegistrarDbRedisAsync() override = default;

    // ... existing methods ...

    std::vector<std::shared_ptr<ExtendedContact>> fetchExpiringContacts(
        const std::chrono::system_clock::time_point& now,
        const std::chrono::seconds& threshold) override;

    void updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) override;

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

} // namespace flexisip 