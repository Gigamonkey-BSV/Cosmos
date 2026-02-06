
#include <Cosmos/REST/REST.hpp>
#include <data/tools/schema.hpp>

#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/electrum_sv.hpp>
#include <gigamonkey/schema/bip_39.hpp>

namespace schema = data::schema;

namespace Cosmos {

    std::istream &operator >> (std::istream &i, master_key_type &x) {
        std::string word;
        i >> word;
        if (!i) return i;
        std::string sanitized = sanitize (word);
        if (sanitized == "single_address") x = master_key_type::single_address;
        else if (sanitized == "hdsequence") x = master_key_type::HD_sequence;
        else if (sanitized == "bip44account") x = master_key_type::BIP44_account;
        else if (sanitized == "bip44master") x = master_key_type::BIP44_master;
        else i.setstate (std::ios::failbit);
        return i;
    }

    restore_request_options::restore_request_options (const args::parsed &) {

        throw data::method::unimplemented {"method RESTORE"};
    }
}
