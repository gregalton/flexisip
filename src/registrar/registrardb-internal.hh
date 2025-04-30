#pragma once

#include "registrar-db.hh"
#include "record.hh"
#include "extended-contact.hh"
#include "flexisip/sofia-wrapper/msg-sip.hh"

namespace flexisip {

class RegistrarDbInternal : public RegistrarDb {
public:
    RegistrarDbInternal(Agent* agent) : RegistrarDb(agent) {}

    // Fix method signatures to match base class
    void fetchExpiringContacts(time_t startTimestamp,
                             float threshold,
                             std::function<void(std::vector<ExtendedContact>&&)>&& callback) const override;

    std::vector<std::shared_ptr<ExtendedContact>> fetchExpiringContacts(
        const std::chrono::system_clock::time_point& time,
        const std::chrono::seconds& threshold) const override;

    void updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) override;

    void doBind(const MsgSip& sip,
                const BindingParameters& parameters,
                const std::shared_ptr<ContactUpdateListener>& listener) override;

    void doClear(const MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) override;

    void doFetch(const SipUri& url, const std::shared_ptr<ContactUpdateListener>& listener) const override;

    void doFetchInstance(const SipUri& url,
                        const std::string& uniqueId,
                        const std::shared_ptr<ContactUpdateListener>& listener) const override;

    void publish(const std::string& topic, const std::string& uid) override;

private:
    // ... existing code ...
};

} // namespace flexisip 