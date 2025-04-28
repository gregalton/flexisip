#pragma once

#include "registrar-db.hh"
#include "record.hh"
#include "extended-contact.hh"
#include <flexisip/registrar/registar-listeners.hh>

namespace flexisip {

/**
 * Listener for handling registration renewal results
 */
class RenewalListener : public ContactUpdateListener {
public:
    void onRecordFound(const std::shared_ptr<Record>& r) override {
        if (r) {
            SLOGI << kLogPrefix << "Successfully renewed registration for " << r->getKey();
        }
    }

    void onError() override {
        SLOGE << kLogPrefix << "Failed to renew registration";
    }

    void onInvalid() override {
        SLOGE << kLogPrefix << "Invalid registration renewal attempt";
    }

    void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) override {
        SLOGI << kLogPrefix << "Contact updated during renewal: " << ec->urlAsString();
    }

private:
    static constexpr const char* kLogPrefix = "[RenewalListener] ";
};

} // namespace flexisip 