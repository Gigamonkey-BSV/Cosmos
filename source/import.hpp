#include "interface.hpp"
#include "Cosmos.hpp"

void command_import (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);

    maybe<std::string> txid_hex;
    p.get (3, "txid", txid_hex);
    if (!bool (txid_hex)) throw exception {2} << "could not read txid of output to import.";
    digest256 txid {*txid_hex};
    if (!txid.valid ()) throw exception {2} << "could not read txid of output to import.";

    maybe<uint32> index;
    p.get (4, "index", index);
    if (!bool (index)) throw exception {2} << "could not read index of output to import.";

    maybe<std::string> wif;
    p.get (5, "wif", wif);
    if (!bool (wif)) throw exception {2} << "could not read key for redeeming output.";
    Bitcoin::secret key {*wif};
    if (!key.valid ()) throw exception {2} << "could not read key for redeeming output.";

    maybe<std::string> script_code_hex;
    p.get (6, "script_code", script_code_hex);
    maybe<bytes> script_code;
    if (bool (script_code_hex)) script_code = *encoding::hex::read (*script_code_hex);

    Bitcoin::outpoint outpoint {txid, *index};
    e.update<void> (update_pending_transactions);

    e.update<void> ([&outpoint, &key, &script_code] (Cosmos::Interface::writable u) {
        const auto k = u.get ().keys ();
        const auto a = u.get ().account ();
        if (!bool (k) || !bool (a)) throw exception {} << "could not load wallet.";

        auto txdb = u.txdb ();
        if (!bool (txdb)) throw exception {} << "could not read database";

        auto [K, A] = import_output (*k, *a, Bitcoin::prevout {outpoint, txdb->output (outpoint)}, key,
            Gigamonkey::pay_to_address::redeem_expected_size (key.Compressed),
            bool (script_code) ? *script_code : bytes {});

        u.set_keys (K);
        u.set_account (A);
    });
}
