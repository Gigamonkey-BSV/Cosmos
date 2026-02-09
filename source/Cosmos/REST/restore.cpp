
#include <Cosmos/REST/REST.hpp>
#include <data/tools/schema.hpp>

#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/electrum_sv.hpp>
#include <gigamonkey/schema/bip_39.hpp>

namespace schema = data::schema;

namespace Cosmos {

    std::ostream &operator << (std::ostream &, master_key_type) {
        throw data::method::unimplemented {" << master_key_type"};
    }

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

    restore_request_options::restore_request_options (const args::parsed &p) {

        // we must have either a key and optional key type, entropy, or words.
        auto [flags, name, opts] = args::validate (p, args::command {
            set<std::string> {}, schema::list::value<Diophant::symbol> (), schema () && command::call_options ()});

        this->Name = name;

        auto [
            derivation_options,
            key_options,
            accounts_param,
            max_lookup_param, _] = opts;

    }
}
