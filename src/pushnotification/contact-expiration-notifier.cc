/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2025 Belledonne Communications SARL, All rights reserved.

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

#include <ostream>

#include "contact-expiration-notifier.hh"

#include "utils/transport/http/http-message.hh"

using namespace std;

namespace flexisip {

namespace pn = pushnotification;

namespace {

constexpr auto kLogPrefix = "ContactExpirationNotifier: ";

// Abstraction to print the relevant device information of a contact
class DeviceInfo {
public:
	const ExtendedContact& contact;
};

ostream& operator<<(ostream& stream, const DeviceInfo& devInfo) {
	const auto& contact = devInfo.contact;
	return stream << "device '" << contact.mKey.str() << "' of user '" << contact.urlAsString() << "'";
}

} // namespace

ContactExpirationNotifier::ContactExpirationNotifier(chrono::seconds interval,
                                                     float lifetimeThreshold,
                                                     const shared_ptr<sofiasip::SuRoot>& root,
                                                     weak_ptr<pn::Service>&& pnService,
                                                     const RegistrarDb& registrar)
    : mLifetimeThreshold(lifetimeThreshold), mTimer(root, interval), mPNService(std::move(pnService)),
      mRegistrar(registrar) {
	// SAFETY: This lambda is safe memory-wise if and only if it doesn't outlive `this`.
	// Which is the case as long as `this` holds the sofiasip::Timer.
	mTimer.setForEver([this] { onTimerElapsed(); });
}

void ContactExpirationNotifier::onTimerElapsed() {
	auto now = std::chrono::system_clock::now();
	auto expiringContacts = mRegistrar.fetchExpiringContacts(now, mLifetimeThreshold);
	
	for (const auto& contact : expiringContacts) {
		if (isDeviceUnresponsive(contact)) {
			if (shouldRenewRegistration(contact)) {
				renewRegistration(contact);
			} else {
				sendWakeUpNotification(contact);
			}
		}
	}
}

bool ContactExpirationNotifier::isDeviceUnresponsive(const std::shared_ptr<ExtendedContact>& contact) {
	auto lastActivity = contact->getLastActivityTime();
	auto now = std::chrono::system_clock::now();
	auto inactivityDuration = std::chrono::duration_cast<std::chrono::minutes>(now - lastActivity);
	
	return inactivityDuration > std::chrono::minutes(5); // 5 minutes of inactivity
}

bool ContactExpirationNotifier::shouldRenewRegistration(const std::shared_ptr<ExtendedContact>& contact) {
	auto renewalCount = contact->getRenewalCount();
	auto lastRenewalTime = contact->getLastRenewalTime();
	auto now = std::chrono::system_clock::now();
	auto timeSinceLastRenewal = std::chrono::duration_cast<std::chrono::hours>(now - lastRenewalTime);
	
	// Allow up to 3 renewals per 24 hours
	return renewalCount < 3 && timeSinceLastRenewal < std::chrono::hours(24);
}

void ContactExpirationNotifier::renewRegistration(const std::shared_ptr<ExtendedContact>& contact) {
	auto listener = std::make_shared<RenewalListener>();
	auto record = std::make_shared<Record>(contact->getKey());
	record->insert(contact);
	
	// Update renewal tracking
	contact->incrementRenewalCount();
	contact->setLastRenewalTime(std::chrono::system_clock::now());
	
	// Perform the renewal
	mRegistrar.bind(record, listener);
}

void ContactExpirationNotifier::sendWakeUpNotification(const ExtendedContact& contact) {
	SLOGI << kLogPrefix << "Sending service push notifications to wake up mobile devices that have passed "
	      << mLifetimeThreshold << " of their expiration time...";
	DeviceInfo devInfo{contact};
	try {
		const auto request = mPNService->makeRequest(pn::PushType::Background, std::make_unique<pn::PushInfo>(contact));
		if (auto* httpRequest = dynamic_cast<HttpMessage*>(request.get())) {
			// We don't want those service notifications overtaking more important call or message
			// notifications, so send with minimum priority
			httpRequest->mPriority.weight = NGHTTP2_MIN_WEIGHT;
		}

		mPNService->sendPush(request);

		SLOGI << kLogPrefix << "Background push notification successfully sent to " << devInfo;
	} catch (const pushnotification::PushNotificationError& e) {
		SLOGD << kLogPrefix << "Register wake-up PN for " << devInfo << " skipped: " << e.what();
	} catch (const exception& e) {
		SLOGE << kLogPrefix << "Could not send register wake-up notification to " << devInfo << ": "
		      << e.what();
	}
}

unique_ptr<ContactExpirationNotifier> ContactExpirationNotifier::make_unique(const GenericStruct& cfg,
                                                                             const shared_ptr<sofiasip::SuRoot>& root,
                                                                             weak_ptr<pn::Service>&& pnService,
                                                                             const RegistrarDb& registrar) {
	auto interval = cfg.get<ConfigInt>("register-wakeup-interval")->read();
	if (interval <= 0) {
		return nullptr;
	}
	float threshold = cfg.get<ConfigInt>("register-wakeup-threshold")->read() / 100.0;

	return std::make_unique<ContactExpirationNotifier>(chrono::minutes(interval), threshold, root, std::move(pnService),
	                                                   registrar);
}

} // namespace flexisip
