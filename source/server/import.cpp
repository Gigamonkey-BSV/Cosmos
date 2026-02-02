
#include "../method.hpp"
#include "import.hpp"
#include <gigamonkey/pay/BEEF.hpp>

net::HTTP::response handle_import (
    server &p, const Diophant::symbol &wallet_name,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

        // we need to find addresses and xpubs that we need to check.

        if (!content_type)
            return error_response (405, method::IMPORT, problem::invalid_parameter,
                "put transaction in body");

        using WIF = Bitcoin::WIF;
        using BEEF = Gigamonkey::BEEF;

        WIF wif;
        BEEF beef;

        if (*content_type == net::HTTP::content::application_octet_stream) {
            beef = BEEF {body};
        } else if (*content_type == net::HTTP::content::application_json) {
            //beef = BEEF {JSON (std::string (data::string (body)))};
        } else return error_response (405, method::IMPORT, problem::invalid_parameter,
                "Transaction should be BEEF format in JSON or octet stream");

        // TODO go through my old code and incorporate other stuff.
        if (!beef.valid ())
            return error_response (405, method::IMPORT, problem::invalid_parameter,
                "Invalid transaction.");

        map<digest160, maybe<HD::BIP_32::pubkey>> unused;
/*
        for (const database::unused &u : p.DB.get_wallet_unused (wallet_name)) {
            if (Bitcoin::address a {u}; a.valid ())
                unused = unused.insert (a.digest (), {});
            else if (HD::BIP_32::pubkey pp {u}; pp.valid ())
                // generate first 3 addresses.
                for (uint32 i = 0; i < 3; i++)
                    unused = unused.insert (pp.derive ({i}).address ().Digest, pp);

        }*/

        // now go through the tx and check for these unused addresses.


        // first we need a function that recognizes output patterns.
        return error_response (501, method::IMPORT, problem::unimplemented);

}
