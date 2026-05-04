#pragma once
#include "glue/GlueTypes.h"
#include <map>
#include <string>

namespace chrono { namespace glue {

class IdentityManager {
public:
    static IdentityManager& instance();

    void set_self(const Identity& id);
    Identity get_self() const;
    void set_active_transport(TransportKind k);

    void add_contact(const std::string& uid, const Identity& id);
    Identity* find_contact(const std::string& uid);
    std::vector<Identity> all_contacts() const;

    // 跨传输查找: uid → 最佳传输层地址
    std::string best_address_for(const std::string& uid, TransportKind preferred) const;

private:
    IdentityManager() = default;
    Identity self_;
    std::map<std::string, Identity> contacts_;
};

} } // namespace
