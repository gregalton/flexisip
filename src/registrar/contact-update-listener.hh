#pragma once

#include "registrar-db.hh"
#include "record.hh"
#include "extended-contact.hh"
#include "registrar-listeners.hh"

namespace flexisip {

class ContactUpdateListener : public RegistrarDbListener {
public:
    virtual ~ContactUpdateListener();
    virtual void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) = 0;
};

} // namespace flexisip 