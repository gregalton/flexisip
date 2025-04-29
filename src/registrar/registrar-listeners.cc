/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "flexisip/registrar/registrar-listeners.hh"
#include <flexisip/registrar/registar-listeners.hh>

namespace flexisip {

RegistrarDbListener::~RegistrarDbListener() {
}

ContactUpdateListener::~ContactUpdateListener() {
}

ContactRegisteredListener::~ContactRegisteredListener() {
}

LocalRegExpireListener::~LocalRegExpireListener() {
}

RegistrarDbStateListener::~RegistrarDbStateListener() {
}

void RenewalListener::onRecordFound(const std::shared_ptr<Record>& r) {
    if (r) {
        SLOGI << kLogPrefix << "Successfully renewed registration for " << r->getKey();
    }
}

void RenewalListener::onError() {
    SLOGE << kLogPrefix << "Failed to renew registration";
}

void RenewalListener::onInvalid() {
    SLOGE << kLogPrefix << "Invalid registration renewal attempt";
}

void RenewalListener::onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) {
    SLOGI << kLogPrefix << "Contact updated during renewal: " << ec->urlAsString();
}

} // namespace flexisip
