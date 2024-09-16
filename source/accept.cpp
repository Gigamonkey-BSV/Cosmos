#include <data/encoding/hex.hpp>
#include <data/encoding/base64.hpp>
#include <gigamonkey/pay/SPV_envelope.hpp>
#include <gigamonkey/pay/BEEF.hpp>
#include <gigamonkey/p2p/checksum.hpp>
#include "interface.hpp"
#include "Cosmos.hpp"

void command_accept (const arg_parser &p) {

    using namespace Cosmos;
    namespace base58 = Gigamonkey::base58;
    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);

    maybe<std::string> payment_string;
    p.get (3, "payment", payment_string);
    if (!bool (payment_string))
        throw exception {2} << "No payment provided";

    // payment can be an SPV proof in BEEF format (preferred), a txid, a transaction, or an SPV envelope (depricated).

    // eventually we will have a list of txs to import.
    list<Bitcoin::transaction> payment;

    // we may have some SPV proofs by the end of this.
    SPV::proof::map proofs;

    // if the txs are not already broadcast then we have
    // a choice whether to accept them or not.
    bool already_broadcast = false;

    // first we try to read a txid.
    if (Bitcoin::TXID input_txid {*payment_string}; input_txid.valid ()) {
        // In this case we assume the tx is already broadcast and we search for it on the network.
        // TODO
        goto ready;
    }

    // try reading an SPV envelope.
    try {
        if (Gigamonkey::SPV_envelope input_envelope {JSON::parse (*payment_string)}; input_envelope.valid ()) {
            // TODO
            goto ready;
        }
    } catch (JSON::exception &) {}

    {
        bytes input_bytes;

        // we try to read the payment in a variety of formats.
        if (maybe<bytes> try_read_hex = encoding::hex::read (*payment_string); bool (try_read_hex)) {
            input_bytes = *try_read_hex;
        } else if (maybe<bytes> try_read_b64 = encoding::base64::read (*payment_string); bool (try_read_b64)) {
            input_bytes = *try_read_b64;
        } if (base58::check try_read_b58 = base58::check::decode (*payment_string); try_read_b58.valid ()) {
            input_bytes = try_read_b58.payload ();
        } else throw exception {} << "could not read payment";

        // now we have two more options. Try to read
        if (Bitcoin::transaction input_tx {input_bytes}; input_tx.valid ()) {

        } else if (BEEF beef {input_bytes}; beef.valid ()) {

        } else throw exception {} << "could not read payment";
    }

    ready:
    throw exception {} << "Commond accept not yet implemented";
}
