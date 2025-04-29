/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2023 Belledonne Communications SARL, All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <bctoolbox/logging.h>

#include "flexisip/sofia-wrapper/su-root.hh"
#include "flexisip/sofia-wrapper/timer.hh"

#include "pushnotification/push-info.hh"
#include "pushnotification/push-notification-error.hh"
#include "pushnotification/service.hh"
#include "registrar/registrar-db.hh"
#include "flexisip/registrar/registrar-listeners.hh"
#include "registrar/record.hh"
#include "registrar/extended-contact.hh"

namespace flexisip {

class PushNotificationService;

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

/**
 * Send wake up push notifications to devices that are nearing their expiration time to let them register again.
 */
class ContactExpirationNotifier : public StatFinishListener {
public:
	ContactExpirationNotifier(const std::shared_ptr<PushNotificationService>& pnService,
	                          RegistrarDb& registrar,
	                          std::chrono::seconds lifetimeThreshold);
	~ContactExpirationNotifier() = default;

	void onTimerElapsed();

	static std::unique_ptr<ContactExpirationNotifier> make_unique(const GenericStruct&,
	                                                              const std::shared_ptr<sofiasip::SuRoot>&,
	                                                              std::weak_ptr<pushnotification::Service>&&,
	                                                              const RegistrarDb&);

private:
	bool isDeviceUnresponsive(const std::shared_ptr<ExtendedContact>& contact);
	bool shouldRenewRegistration(const std::shared_ptr<ExtendedContact>& contact);
	void renewRegistration(const std::shared_ptr<ExtendedContact>& contact);
	void sendWakeUpNotification(const std::shared_ptr<ExtendedContact>& contact);

	std::weak_ptr<PushNotificationService> mPNService;
	RegistrarDb& mRegistrar;
	const std::chrono::seconds mLifetimeThreshold;
	static constexpr const char* kLogPrefix = "[ContactExpirationNotifier] ";
};

} // namespace flexisip
