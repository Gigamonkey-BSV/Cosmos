
#include "../Cosmos.hpp"
#include "import.hpp"
#include <gigamonkey/pay/BEEF.hpp>

using namespace Cosmos;

struct retriever {

};

net::HTTP::response handle_import (
    server &p, const Diophant::symbol &wallet_name,
    const dispatch<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    import_request_options opts {wallet_name, query, content_type, body};

    // TODO
    // if opts is not valid, we need to return an error.

    // open a channel to whatsonchain.

    // index all txs
    map<Bitcoin::TxID, list<size_t>> txi;

    // we try to fill in all transactions that are not complete as BEEFs.
    int ind = 0;
    for (const auto &t : opts.Txs) t.visit ([&](const auto &n) {
        if constexpr (std::is_same_v<std::decay_t<decltype (n)>, Bitcoin::TxID>) {

        } else {

        }
    });

    // add all txids in outpoints not already in the list.

    // import all txs into the database.

    // first we figure out how many outpoints we have been given because if there's one, it's a special case.
    if (opts.Outpoints.size () == 1) {

    }

    // if there are no outpoints go through them all outputs.

    // go through all outpoints.

    // finally, let's examine any keys that we haven't looked at yet.



    // we need to find addresses and xpubs that we need to check.

    if (!content_type)
        return error_response (405, command::IMPORT, command::problem::invalid_parameter,
            "put transaction in body");

    using WIF = Bitcoin::WIF;
    using BEEF = Gigamonkey::BEEF;

    WIF wif;
    BEEF beef;

    if (*content_type == net::HTTP::content::application_octet_stream) {
        beef = BEEF {body};
    } else if (*content_type == net::HTTP::content::application_json) {
        //beef = BEEF {JSON (std::string (data::string (body)))};
    } else return error_response (405, command::IMPORT, command::problem::invalid_parameter,
            "Transaction should be BEEF format in JSON or octet stream");

    // TODO go through my old code and incorporate other stuff.
    if (!beef.valid ())
        return error_response (405, command::IMPORT, command::problem::invalid_parameter,
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
    return error_response (501, command::IMPORT, command::problem::unimplemented);

}
