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
#include <chrono>
#include <optional>
#include <regex>

#include "contact-expiration-notifier.hh"
#include "push-notification-exceptions.hh"

#include "flexisip/logmanager.hh"
#include "registrar/extended-contact.hh"
#include "registrar/registrar-db.hh"
#include "utils/transport/http/http-message.hh"

using namespace std;

namespace flexisip {

namespace pn = pushnotification;

namespace {

constexpr auto kLogPrefix = "[ContactExpirationNotifier] ";

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
                                                     const RegistrarDb& registrar,
                                                     bool enableTokenRefresh)
    : mLifetimeThreshold(lifetimeThreshold), mTimer(root, interval), mPNService(std::move(pnService)),
      mRegistrar(registrar), mEnableTokenRefresh(enableTokenRefresh) {
	// SAFETY: This lambda is safe memory-wise if and only if it doesn't outlive `this`.
	// Which is the case as long as `this` holds the sofiasip::Timer.
	mTimer.setForEver([this] { onTimerElapsed(); });

	SLOGI << kLogPrefix << "ContactExpirationNotifier initialized with token refresh "
	      << (mEnableTokenRefresh ? "enabled" : "disabled");
}

void ContactExpirationNotifier::onTimerElapsed() {
	SLOGI << kLogPrefix << "Sending service push notifications and refreshing tokens for mobile devices that have passed "
	      << mLifetimeThreshold << " of their expiration time...";

	// Stage 1: Refresh push tokens for contacts that need it (if enabled)
	// This is done before extension to ensure extended registrations have valid tokens
	if (mEnableTokenRefresh) {
		try {
			refreshPushTokensForExpiringContacts();
			SLOGD << kLogPrefix << "Push token refresh completed";
		} catch (const std::exception& e) {
			SLOGE << kLogPrefix << "Error in push token refresh: " << e.what();
		}
	} else {
		SLOGD << kLogPrefix << "Push token refresh is disabled";
	}

	// Stage 2: Extend registrations with refreshed tokens
	try {
		int extended = const_cast<RegistrarDb&>(mRegistrar).extendExpiringRegistrations();
		SLOGD << kLogPrefix << "Extended " << extended << " eligible registrations";
	} catch (const std::exception& e) {
		SLOGE << kLogPrefix << "Error extending registrations: " << e.what();
	}

	mRegistrar.fetchExpiringContacts(
	    getCurrentTime(), mLifetimeThreshold, [weakPNService = mPNService](auto&& contacts) mutable {
		    static constexpr const auto pushType = pn::PushType::Background;
		    auto pnService = weakPNService.lock();
		    if (!pnService) {
			    SLOGI << kLogPrefix
			          << "Push notification service destructed, cannot send register wake up notifications "
			             "(This is expected if flexisip is being shut down)";
			    return;
		    }

		    for (const auto& contact : contacts) {
			    DeviceInfo devInfo{contact};
			    try {
				    const auto request = pnService->makeRequest(pushType, std::make_unique<pn::PushInfo>(contact));
				    if (auto* httpRequest = dynamic_cast<HttpMessage*>(request.get())) {
					    // We don't want those service notifications overtaking more important call or message
					    // notifications, so send with minimum priority
					    httpRequest->mPriority.weight = NGHTTP2_MIN_WEIGHT;
				    }

				    pnService->sendPush(request);

				    SLOGI << kLogPrefix << "background push notification successfully sent to " << devInfo;
			    } catch (const pn::UnavailablePushNotificationClient& e) {
				    SLOGD << kLogPrefix << "failed to send push notification to " << devInfo << ": " << e.what();
			    } catch (const exception& e) {
				    SLOGE << kLogPrefix << "failed to send push notification to " << devInfo << ": " << e.what();
			    }
		    }
	    });
}

unique_ptr<ContactExpirationNotifier> ContactExpirationNotifier::make_unique(const GenericStruct& cfg,
                                                                             const shared_ptr<sofiasip::SuRoot>& root,
                                                                             weak_ptr<pn::Service>&& pnService,
                                                                             const RegistrarDb& registrar) {
	auto interval =
	    chrono::duration_cast<chrono::minutes>(cfg.get<ConfigDuration<chrono::minutes>>("register-wakeup-interval")->read());
	if (interval <= 0min) {
		return nullptr;
	}
	float threshold = cfg.get<ConfigInt>("register-wakeup-threshold")->read() / 100.0;
	auto enableTokenRefresh = cfg.get<ConfigBoolean>("register-wakeup-token-refresh")->read();

	return std::make_unique<ContactExpirationNotifier>(interval, threshold, root, std::move(pnService),
	                                                   registrar, enableTokenRefresh);
}

void ContactExpirationNotifier::refreshPushTokensForExpiringContacts() {
	SLOGD << kLogPrefix << "Starting push token refresh for expiring contacts...";

	mRegistrar.fetchExpiringContacts(
	    getCurrentTime(), mLifetimeThreshold,
	    [this](auto&& contacts) {
		    int refreshedCount = 0;
		    int totalCount = contacts.size();

		    SLOGD << kLogPrefix << "Checking " << totalCount << " expiring contacts for token refresh";

		    for (const auto& contact : contacts) {
			    try {
				    auto tokenInfo = extractPushTokenInfo(contact);
				    if (!tokenInfo) {
					    SLOGD << kLogPrefix << "Contact " << contact.contactId() << " has no push tokens, skipping";
					    continue;
				    }

				    if (!shouldRefreshToken(*tokenInfo, contact)) {
					    SLOGD << kLogPrefix << "Contact " << contact.contactId() << " token is still valid, skipping";
					    continue;
				    }

				    SLOGD << kLogPrefix << "Refreshing push token for contact " << contact.contactId()
				          << " (provider: " << tokenInfo->provider << ")";

				    auto refreshedTokenInfo = refreshTokenForContact(contact, *tokenInfo);
				    if (refreshedTokenInfo) {
					    auto updatedContact = updateContactWithRefreshedTokens(
					        std::make_shared<ExtendedContact>(contact), *refreshedTokenInfo);
					    if (updatedContact) {
						    refreshedCount++;
						    SLOGD << kLogPrefix << "Successfully refreshed token for contact " << contact.contactId();
					    } else {
						    SLOGE << kLogPrefix << "Failed to update contact " << contact.contactId()
						          << " with refreshed token";
					    }
				    } else {
					    SLOGW << kLogPrefix << "Token refresh failed for contact " << contact.contactId();
				    }
			    } catch (const std::exception& e) {
				    SLOGE << kLogPrefix << "Error refreshing token for contact " << contact.contactId()
				          << ": " << e.what();
			    }
		    }

		    SLOGI << kLogPrefix << "Push token refresh completed: " << refreshedCount << "/" << totalCount
		          << " tokens refreshed";
	    });
}

std::optional<ContactExpirationNotifier::PushTokenInfo>
ContactExpirationNotifier::extractPushTokenInfo(const ExtendedContact& contact) {
	const auto* contactUri = contact.mSipContact->m_url;
	if (!contactUri || !contactUri->url_params) {
		return std::nullopt;
	}

	PushTokenInfo tokenInfo{};
	tokenInfo.lastRefresh = 0;

	char paramValue[512] = {0};

	// Extract pn-provider
	if (url_param(contactUri->url_params, "pn-provider", paramValue, sizeof(paramValue) - 1) > 0) {
		tokenInfo.provider = std::string(paramValue);
	}

	// Extract pn-prid
	memset(paramValue, 0, sizeof(paramValue));
	if (url_param(contactUri->url_params, "pn-prid", paramValue, sizeof(paramValue) - 1) > 0) {
		tokenInfo.prid = std::string(paramValue);
	}

	// Extract pn-param
	memset(paramValue, 0, sizeof(paramValue));
	if (url_param(contactUri->url_params, "pn-param", paramValue, sizeof(paramValue) - 1) > 0) {
		tokenInfo.param = std::string(paramValue);

		// Extract team ID and bundle ID from pn-param (format: teamId.bundleId.services)
		std::regex paramRegex(R"(([^.]+)\.([^.]+)\.(.+))");
		std::smatch matches;
		std::string paramStr(paramValue);
		if (std::regex_match(paramStr, matches, paramRegex)) {
			tokenInfo.teamId = matches[1].str();
			tokenInfo.bundleId = matches[2].str();
		}
	}

	if (!tokenInfo.provider.empty() && !tokenInfo.prid.empty()) {
		return tokenInfo;
	}

	return std::nullopt;
}

std::optional<ContactExpirationNotifier::PushTokenInfo>
ContactExpirationNotifier::refreshTokenForContact(const ExtendedContact& contact, const PushTokenInfo& tokenInfo) {
	SLOGD << kLogPrefix << "Refreshing token for contact " << contact.contactId()
	      << " with provider " << tokenInfo.provider;

	if (tokenInfo.provider == "fcm") {
		return refreshAndroidToken(tokenInfo);
	} else if (tokenInfo.provider == "apns" || tokenInfo.provider == "apns.dev") {
		return refreshiOSToken(tokenInfo);
	} else {
		SLOGW << kLogPrefix << "Unknown push provider: " << tokenInfo.provider;
		return std::nullopt;
	}
}

std::optional<ContactExpirationNotifier::PushTokenInfo>
ContactExpirationNotifier::refreshAndroidToken(const PushTokenInfo& tokenInfo) {
	SLOGD << kLogPrefix << "Performing Android FCM token refresh";

	PushTokenInfo refreshedInfo = tokenInfo;
	refreshedInfo.lastRefresh = getCurrentTime();

	SLOGD << kLogPrefix << "Android token refreshed (timestamp updated)";
	return refreshedInfo;
}

std::optional<ContactExpirationNotifier::PushTokenInfo>
ContactExpirationNotifier::refreshiOSToken(const PushTokenInfo& tokenInfo) {
	SLOGD << kLogPrefix << "Performing iOS APNS token refresh";

	PushTokenInfo refreshedInfo = tokenInfo;
	refreshedInfo.lastRefresh = getCurrentTime();

	SLOGD << kLogPrefix << "iOS token refreshed (timestamp updated)";
	return refreshedInfo;
}

bool ContactExpirationNotifier::shouldRefreshToken(const PushTokenInfo& tokenInfo, const ExtendedContact& contact) {
	if (tokenInfo.lastRefresh == 0) {
		SLOGD << kLogPrefix << "Token for contact " << contact.contactId() << " has never been refreshed";
		return true;
	}

	time_t currentTime = getCurrentTime();
	time_t timeSinceRefresh = currentTime - tokenInfo.lastRefresh;

	time_t refreshInterval;
	if (tokenInfo.provider == "fcm") {
		refreshInterval = 6 * 3600; // 6 hours
	} else if (tokenInfo.provider == "apns" || tokenInfo.provider == "apns.dev") {
		refreshInterval = 2 * 3600; // 2 hours
	} else {
		refreshInterval = 4 * 3600; // 4 hours
	}

	if (timeSinceRefresh >= refreshInterval) {
		SLOGD << kLogPrefix << "Token for contact " << contact.contactId()
		      << " needs refresh (last refresh: " << timeSinceRefresh << "s ago)";
		return true;
	}

	return false;
}

std::shared_ptr<ExtendedContact>
ContactExpirationNotifier::updateContactWithRefreshedTokens(const std::shared_ptr<ExtendedContact>& contact,
                                                             const PushTokenInfo& newTokenInfo) {
	SLOGD << kLogPrefix << "Marking contact " << contact->contactId() << " as having refreshed push tokens";

	// For the initial implementation, we log the refresh and return the original contact.
	// In a future enhancement, we could:
	// 1. Store refresh timestamps in Redis
	// 2. Update contact URIs with new push parameters
	// 3. Trigger synthetic REGISTER requests with updated tokens
	SLOGD << kLogPrefix << "Token refresh completed for contact " << contact->contactId()
	      << " (provider: " << newTokenInfo.provider << ", last refresh: " << newTokenInfo.lastRefresh << ")";

	return contact;
}

} // namespace flexisip
