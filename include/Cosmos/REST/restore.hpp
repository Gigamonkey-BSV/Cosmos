#ifndef SERVER_RESTORE
#define SERVER_RESTORE

#include "generate.hpp"

namespace Cosmos {

    enum class master_key_type {
        invalid,
        single_address,
        // a single sequnce of keys. This could be from any wallet but
        // is not a complete wallet.
        HD_sequence,

        // In this case, there will be receive and change derivations.
        BIP44_account,

        // in this case, we also need a coin type or wallet type.
        BIP44_master,
    };

    std::ostream &operator << (std::ostream &, master_key_type);
    std::istream &operator >> (std::istream &, master_key_type &);

    struct restore_request_options : generate_request_options {

        net::HTTP::request request (const UTF8 & = "localhost") {
            throw data::method::unimplemented {"restore_request_options -> HTTP request"};
        }

        restore_request_options (
            Diophant::symbol wallet_name, map<UTF8, UTF8> query,
            const maybe<net::HTTP::content> &content_type,
            const data::bytes &body);

        restore_request_options (const args::parsed &);

        bool valid () const;
    };

    struct restore_result {};
}

#endif
