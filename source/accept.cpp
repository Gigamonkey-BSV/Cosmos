#include <data/encoding/hex.hpp>
#include <data/encoding/base64.hpp>
#include <gigamonkey/pay/SPV_envelope.hpp>
#include <gigamonkey/pay/BEEF.hpp>
#include <gigamonkey/p2p/checksum.hpp>
#include "interface.hpp"
#include "Cosmos.hpp"

namespace Cosmos {
    struct request_integrator {
        map<Bitcoin::TXID, extended_transaction> Payment;

        // all outputs that will be ours if we accept this payment.
        map<Bitcoin::outpoint, redeemable> Out;

        list<tuple<string, payments::request, Bitcoin::satoshi>> RequestsSatisfied;

        Bitcoin::satoshi search_script (const bytes &script, const signing &sg) {
            int outputs_found {0};
            Bitcoin::satoshi total_value {0};

            // does the transaction pay to this address?
            for (const auto &[txid, extx] : Payment) {
                Bitcoin::index i {0};
                for (const auto &op : extx.Outputs) {
                    if (op.Script == script) {
                        total_value += op.Value;
                        Out = Out.insert (Bitcoin::outpoint {txid, i++}, redeemable {op, sg});
                        outputs_found++;
                    }
                    i++;
                }

            }

            if (outputs_found > 1) std::cout << "WARNING: more than one output found with the same script";
            return total_value;
        }

        request_integrator (list<extended_transaction> txs, const map<string, payments::redeemable> &pmts) {
            for (const auto &extx : txs) Payment = Payment.insert (extx.id (), extx);

            // check payments to see if we can figure out which payment request this satisfies.
            for (const auto &[id, re] : pmts)
                // we have three types of payment requests. Address, pubkey, and xpub.
                if (Bitcoin::address addr {id}; addr.valid ()) {

                    Bitcoin::satoshi value_paid = search_script (
                        pay_to_address {addr.digest ()}.script (),
                        // assuming here that the pubkey is compressed, since we don't know.
                        signing {{re.Derivation}, pay_to_address::redeem_expected_size ()});

                    if (value_paid > 0) RequestsSatisfied <<= {id, re.Request.Value, value_paid};

                } else if (Bitcoin::pubkey pk {id}; pk.valid ()) {

                    Bitcoin::satoshi value_paid = search_script (
                        pay_to_pubkey {pk}.script (),
                        // assuming here that the pubkey is compressed, since we don't know.
                        signing {{re.Derivation}, pay_to_pubkey::redeem_expected_size ()});

                    if (value_paid > 0) RequestsSatisfied <<= {id, re.Request.Value, value_paid};

                } else if (HD::BIP_32::pubkey xpub {id}; xpub.valid ()) {

                    int max_look_ahead = options::DefaultMaxLookAhead;
                    bool satisfied = false;
                    Bitcoin::satoshi value_paid {0};
                    int derivation_value = -1;
                    int last = -2;

                    do {
                        // check a bunch of addresses.
                        Bitcoin::satoshi val = search_script (
                            pay_to_address {
                                xpub.derive (derivation_value == -1 ? list<uint32> {} :
                                    list<uint32> {static_cast<uint32> (derivation_value)}).address ().Digest}.script (),
                            // assuming here that the pubkey is compressed, since we don't know.
                            signing {{re.Derivation}, pay_to_address::redeem_expected_size ()});

                        if (val > 0) {
                            satisfied = true;
                            value_paid += val;
                            last = derivation_value;
                        }
                    } while (derivation_value - last < max_look_ahead);

                    RequestsSatisfied <<= {id, re.Request.Value, value_paid};

                } else throw exception {} << "unrecognized payment request type";
        }

        Bitcoin::satoshi total_value () const;
    };
}

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
    list<extended_transaction> payment;

    // if the txs are not already broadcast then we have
    // a choice whether to accept them or not.
    bool already_broadcast = false;

    // first we try to read a txid.
    if (Bitcoin::TXID input_txid {*payment_string}; input_txid.valid ()) {
        // In this case we assume the tx is already broadcast and we search for it on the network.
        throw exception {} << "You have entered a TXID. Unfortunately, this option is not yet supported";
        // TODO
        goto ready;
    }

    // try reading an SPV envelope.
    try {
        if (Gigamonkey::SPV_envelope input_envelope {JSON::parse (*payment_string)}; input_envelope.valid ()) {
            throw exception {} << "You have entered an SPV envelope. Unfortunately, this option is not yet supported";
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
            throw exception {} << "You have entered a transaction. Unfortunately, this option is not yet supported";
            // TODO
        } else if (BEEF beef {input_bytes}; beef.valid ()) {
            payment = e.update<list<extended_transaction>> ([&beef] (Cosmos::Interface::writable w) {
                SPV::proof p = beef.read_SPV_proof (*w.local_txdb ());
                if (!p.validate (*w.local_txdb ())) throw exception {} << "failed to validate SPV proof";
                std::cout << "Validated SPV proof in BEEF format" << std::endl;
                return list<extended_transaction> (p);
            });
        } else throw exception {} << "could not read payment";
    }

    ready:
    if (payment.size () == 0) throw exception {} << "no payment found";
    std::cout << payment.size () << " payment transactions found" << std::endl;

    // TODO what if one transaction is valid and another isn't?
    for (const auto &extx : payment)
        if (!extx.valid ()) throw exception {} << "tx " << extx.id () << " is not valid ";

    std::cout << "When this command is finished, we will look at outstanding payment requests and attempt to determine which "
        "is satisfied by the payment and provide an option as to whether to accept and broadcast it." << std::endl;
    throw exception {} << "Commond accept implementation is not complete.";

    e.update<void> ([&payment] (Cosmos::Interface::writable u) {
        const auto *pay = u.get ().payments ();
        auto requests = pay->Requests;
        request_integrator tg {payment, requests};

        Bitcoin::satoshi total_value = tg.total_value ();
        std::cout << "  " << total_value << " sats found for you. If you accept this payment then your wallet will have " <<
            (u.get ().account ()->value () + total_value) << " sats." << std::endl;

        std::cout << "  This payment satisfies " << tg.RequestsSatisfied.size () <<
            " payment request" << (tg.RequestsSatisfied.size () != 1 ? "s" : "") << ":" << std::endl;
        for (const auto &[str, req, tot] : tg.RequestsSatisfied) {
            std::cout << "    " << str << " paying " << tot;
            if (bool (req.Amount)) std::cout << " for " << *req.Amount << " sats requested";
            std::cout << "." << std::endl;
        }

        if (!get_user_yes_or_no ("Do you want to accept this payment?")) throw exception {} << "You chose not to accept this payment.";

        // collect information about all new outputs.
        map<Bitcoin::TXID, list<entry<Bitcoin::index, redeemable>>> diffs;

        for (const auto &[op, re] : tg.Out)
            diffs = diffs.insert (op.Digest, {{op.Index, re}},
                [] (const list<entry<Bitcoin::index, redeemable>> &ol, const list<entry<Bitcoin::index, redeemable>> &nl)
                    -> list<entry<Bitcoin::index, redeemable>> {
                    return ol + nl;
                });

        // broadcast the transactions.
        for (const auto &[txid, extx] : tg.Payment)
            u.broadcast (extx, account_diff {txid,
                fold ([] (map<Bitcoin::index, redeemable> m, entry<Bitcoin::index, redeemable> e) -> map<Bitcoin::index, redeemable> {
                    return m.insert (e);
                }, map<Bitcoin::index, redeemable> {}, diffs[txid]), {}});

        for (const auto &[id, a, b] : tg.RequestsSatisfied) requests = requests.remove (id);

        u.set_payments (payments {requests, pay->Proposals});

    });

}
