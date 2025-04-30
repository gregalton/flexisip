#pragma once

#include "registrar/registrar-db.hh"
#include "registrar/record.hh"
#include "registrar/extended-contact.hh"
#include "sofia-wrapper/msg-sip.hh"

namespace flexisip {

class RegistrarDbListener {
public:
    virtual ~RegistrarDbListener() = default;
};

class ContactUpdateListener {
public:
    virtual ~ContactUpdateListener() = default;
    virtual void onRecordFound(const std::shared_ptr<Record>& r) = 0;
    virtual void onError() = 0;
    virtual void onInvalid() = 0;
    virtual void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) = 0;
};

class ContactRegisteredListener {
public:
    virtual ~ContactRegisteredListener() = default;
    virtual void onContactRegistered(const std::shared_ptr<Record>& r, const std::string& uid) = 0;
};

class LocalRegExpireListener {
public:
    virtual ~LocalRegExpireListener() = default;
    virtual void onLocalRegExpireUpdated(unsigned int count) = 0;
};

class RegistrarDbStateListener {
public:
    virtual ~RegistrarDbStateListener() = default;
    virtual void onRegistrarDbWritable(bool writable) = 0;
};

/**
 * Listener for handling registration renewal results
 */
class RenewalListener : public ContactUpdateListener {
public:
    static constexpr const char* kLogPrefix = "RenewalListener: ";
    void onRecordFound(const std::shared_ptr<Record>& r) override;
    void onError() override;
    void onInvalid() override;
    void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) override;
};

} // namespace flexisip 