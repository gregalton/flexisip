#include "registrardb-internal.hh"
#include "registrar-db.hh"
#include "record.hh"
#include "extended-contact.hh"
#include "flexisip/sofia-wrapper/msg-sip.hh"

namespace flexisip {

// Fix doFetch implementation to be const
void RegistrarDbInternal::doFetch(const SipUri& url, const std::shared_ptr<ContactUpdateListener>& listener) const {
    // ... existing code ...
}

// Fix doFetchInstance implementation to be const
void RegistrarDbInternal::doFetchInstance(const SipUri& url,
                                        const std::string& uniqueId,
                                        const std::shared_ptr<ContactUpdateListener>& listener) const {
    // ... existing code ...
}

// Fix doClear implementation to handle listener correctly
void RegistrarDbInternal::doClear(const MsgSip& sip, const std::shared_ptr<ContactUpdateListener>& listener) {
    // ... existing code ...
}

// Fix publish implementation to handle parameters correctly
void RegistrarDbInternal::publish(const std::string& topic, const std::string& uid) {
    // ... existing code ...
}

} // namespace flexisip 