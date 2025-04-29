#pragma once

#include "registrar-db.hh"
#include "record.hh"
#include "extended-contact.hh"
#include "registrar-interfaces.hh"
#include "contact-update-listener.hh"

namespace flexisip {

/**
 * Listener for handling registration renewal results
 */
class RenewalListener : public ContactUpdateListener {
public:
    void onRecordFound(const std::shared_ptr<Record>& r) override;
    void onError() override;
    void onInvalid() override;
    void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) override;

private:
    static constexpr const char* kLogPrefix = "[RenewalListener] ";
};

} // namespace flexisip 