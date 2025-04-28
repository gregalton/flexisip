class RegistrarDbRedisAsync : public RegistrarDb {
public:
    // ... existing code ...

    std::vector<std::shared_ptr<ExtendedContact>> fetchExpiringContacts(
        const std::chrono::system_clock::time_point& time,
        const std::chrono::seconds& threshold) override {
        std::vector<std::shared_ptr<ExtendedContact>> expiringContacts;
        
        // Convert time_point to seconds since epoch
        auto time_seconds = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
        
        // Get all keys matching the pattern for contacts
        auto keys = mRedisClient->keys("contact:*");
        
        for (const auto& key : keys) {
            // Get contact data from Redis
            auto contactData = mRedisClient->hgetall(key);
            if (contactData.empty()) continue;
            
            // Parse expiration time from contact data
            auto it = contactData.find("expires");
            if (it == contactData.end()) continue;
            
            auto expires = std::stoll(it->second);
            auto time_until_expiry = expires - time_seconds;
            
            // If contact is expiring within the threshold, add it to the list
            if (time_until_expiry <= threshold.count() && time_until_expiry > 0) {
                auto contact = std::make_shared<ExtendedContact>();
                
                // Populate contact with data from Redis
                sofiasip::Home home;
                
                // Create SIP contact from URL
                auto urlStr = contactData["url"];
                auto sipContact = sip_contact_create(home.home(), (const url_string_t*)urlStr.c_str(), nullptr, nullptr);
                if (!sipContact) continue;
                
                contact->mSipContact = sipContact;
                contact->mExpires = std::chrono::seconds(expires - time_seconds);
                contact->mCallId = contactData["call_id"];
                contact->mCSeq = std::stoi(contactData["cseq"]);
                
                // Parse path if exists
                auto pathIt = contactData.find("path");
                if (pathIt != contactData.end()) {
                    std::istringstream pathStream(pathIt->second);
                    std::string path;
                    while (std::getline(pathStream, path, ',')) {
                        contact->mPath.push_back(path);
                    }
                }
                
                expiringContacts.push_back(contact);
            }
        }
        
        return expiringContacts;
    }

    void updateContactActivity(const std::shared_ptr<ExtendedContact>& contact) override {
        if (!contact) return;
        
        // Update last activity time in Redis
        auto key = "contact:" + contact->mKey;
        auto now = std::chrono::system_clock::now();
        auto now_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        
        // Update contact data in Redis
        std::unordered_map<std::string, std::string> contactData;
        contactData["last_activity"] = std::to_string(now_seconds);
        contactData["url"] = url_as_string(home.home(), contact->mSipContact->m_url);
        contactData["expires"] = std::to_string(contact->mExpires.count());
        contactData["call_id"] = contact->mCallId;
        contactData["cseq"] = std::to_string(contact->mCSeq);
        
        // Store path as comma-separated string
        std::stringstream pathStream;
        for (size_t i = 0; i < contact->mPath.size(); ++i) {
            if (i > 0) pathStream << ",";
            pathStream << contact->mPath[i];
        }
        contactData["path"] = pathStream.str();
        
        mRedisClient->hmset(key, contactData);
    }

    // ... rest of existing code ...
}; 