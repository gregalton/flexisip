/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2024 Belledonne Communications SARL, All rights reserved.

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

#include <exception>
#include <optional>

#include <bctoolbox/logging.h>

#include "flexisip/sofia-wrapper/su-root.hh"
#include "flexisip/sofia-wrapper/timer.hh"

#include "pushnotification/push-info.hh"
#include "pushnotification/service.hh"
#include "registrar/registrar-db.hh"

namespace flexisip {

/**
 * Send wake up push notifications to devices that are nearing their expiration time to let them register again.
 * Also handles push token refresh for extended registrations to maintain valid tokens.
 */
class ContactExpirationNotifier {
public:
	ContactExpirationNotifier(std::chrono::seconds interval,
	                          float lifetimeThreshold,
	                          const std::shared_ptr<sofiasip::SuRoot>&,
	                          std::weak_ptr<pushnotification::Service>&&,
	                          const RegistrarDb&,
	                          bool enableTokenRefresh = true);

	void onTimerElapsed();

	static std::unique_ptr<ContactExpirationNotifier> make_unique(const GenericStruct&,
	                                                              const std::shared_ptr<sofiasip::SuRoot>&,
	                                                              std::weak_ptr<pushnotification::Service>&&,
	                                                              const RegistrarDb&);

	/**
	 * Refresh push tokens for contacts that are about to expire or have been extended.
	 * This ensures extended registrations maintain valid push tokens,
	 * especially important for Android FCM tokens.
	 */
	void refreshPushTokensForExpiringContacts();

private:
	const float mLifetimeThreshold; // Notify devices that have passed that proportion of their time to live
	sofiasip::Timer mTimer;
	std::weak_ptr<pushnotification::Service> mPNService;
	const RegistrarDb& mRegistrar;
	const bool mEnableTokenRefresh; // Whether to perform push token refresh

	// Push token refresh functionality
	struct PushTokenInfo {
		std::string provider;    // "fcm", "apns", "apns.dev"
		std::string prid;        // Push registration ID (token)
		std::string param;       // Push parameters
		std::string teamId;      // iOS team ID
		std::string bundleId;    // App bundle identifier
		time_t lastRefresh;      // Last token refresh time
	};

	/**
	 * Extract push notification parameters from a contact URI.
	 */
	std::optional<PushTokenInfo> extractPushTokenInfo(const ExtendedContact& contact);

	/**
	 * Refresh push tokens for a specific contact.
	 */
	std::optional<PushTokenInfo> refreshTokenForContact(const ExtendedContact& contact, const PushTokenInfo& tokenInfo);

	/**
	 * Update contact with refreshed push token parameters.
	 */
	std::shared_ptr<ExtendedContact> updateContactWithRefreshedTokens(const std::shared_ptr<ExtendedContact>& contact,
	                                                                   const PushTokenInfo& newTokenInfo);

	/**
	 * Platform-specific token refresh logic.
	 */
	std::optional<PushTokenInfo> refreshAndroidToken(const PushTokenInfo& tokenInfo);
	std::optional<PushTokenInfo> refreshiOSToken(const PushTokenInfo& tokenInfo);

	/**
	 * Check if a token needs refreshing based on platform-specific criteria.
	 */
	bool shouldRefreshToken(const PushTokenInfo& tokenInfo, const ExtendedContact& contact);
};

} // namespace flexisip
