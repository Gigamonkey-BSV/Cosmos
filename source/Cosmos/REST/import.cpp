
#include <Cosmos/REST/import.hpp>
#include <Cosmos/REST/problem.hpp>

namespace Cosmos {

    auto import_options_pattern =
        // We will have at least one --tx, at least one --outpoint, or at least one --key
        schema::map::keys<import_request_options::tx> ("tx") &&
            schema::map::keys<Bitcoin::outpoint> ("outpoint") &&
            schema::map::keys<Bitcoin::WIF> ("key") &&
            *schema::map::key<uint32> ("index");

    import_request_options::import_request_options (const args::parsed &p) {

        auto [flags, args, opts] = args::validate (p,
            args::command {set<std::string> {}, // no flags
                // the first two args are the original command and the method.
                schema::list::blank () + schema::list::blank () +
                // this is the name of the wallet.
                schema::list::value<Diophant::symbol> (),
                import_options_pattern && command::call_options ()});

        // now we know that there are no unexpected keys.

        auto [txs, outpoints, keys, index, _] = opts;

        // check that we have at least one of tx, outpoint, or keys.
        if (txs.size () == 0 && outpoints.size () == 0 && keys.size () == 0)
            throw data::exception {} << "Need at least one of tx, outpoint, or key";

        // if we have index, then we should have no outpoints and exactly one tx.
        if (index) {
            if (outpoints.size () != 0 || txs.size () != 1)
                throw data::exception {} << "If 'index' is given, no 'outpoint' is allowed and only one 'tx' is allowed.";

            Outpoints = {Bitcoin::outpoint {txs.first ().visit ([] (const auto &n) {
                if constexpr (std::is_same_v<std::decay_t<decltype (n)>, Bitcoin::TxID>) {
                    return n;
                } else if constexpr (std::is_same_v<std::decay_t<decltype (n)>, BEEF>) {
                    return data::last (n.Transactions).id ();
                } else {
                    return n.id ();
                }
            }), *index}};
        } else Outpoints = outpoints;

        Name = std::get<2> (args);

        Txs = txs;
        Keys = Keys;

    }

    list<import_request_options::tx> read_body (const bytes &b) {
        data::iterator_reader r {b.begin (), b.end ()};
        list<import_request_options::tx> txs;
        try {
            while (true) {
                if (r.empty ()) break;
                uint32_little version;
                r >> version;

                // this is a BEEF
                if (version > 0xEFBE0001) {
                    BEEF h;
                    h.Version = version;
                    Bitcoin::var_sequence<BEEF::transaction>::read
                    (Bitcoin::var_sequence<Merkle::BUMP>::read (r, h.BUMPs), h.Transactions);
                    txs <<= import_request_options::tx {h};
                } else {
                    Bitcoin::transaction t;
                    t.Version = int32_little (version);
                    r >> Bitcoin::var_sequence<Bitcoin::input> {t.Inputs} >> Bitcoin::var_sequence<Bitcoin::output> {t.Outputs} >> t.LockTime;
                    txs <<= import_request_options::tx {t};
                }
            }
        } catch (const data::end_of_stream &) {
            throw command::exception {400, command::problem::invalid_content_type, command::IMPORT};
        }

        if (r.Begin != r.End) throw command::exception {400, command::problem::invalid_content_type, command::IMPORT};
        return txs;
    }

    list<import_request_options::tx> read_body (
        const maybe<net::HTTP::content> &content_type,
        const data::bytes &body) {

        if (content_type) {
            if (*content_type == net::HTTP::content::application_octet_stream)
                return read_body (body);
            else if (*content_type == net::HTTP::content::text_plain) {
                string b {body};

                maybe<bytes> hex = encoding::hex::read (b);
                if (hex) return read_body (*hex);

                Bitcoin::TxID t;
                std::stringstream x {b};
                x >> t;
                if (t.valid ()) return {t};
            } else throw command::exception {400, command::problem::invalid_content_type, command::IMPORT};
        }

        return {};
    }

    import_request_options::import_request_options (
        const Diophant::symbol &wallet_name,
        const dispatch<UTF8, UTF8> query,
        const maybe<net::HTTP::content> &content_type,
        const data::bytes &body): Name {wallet_name} {

        auto [txs, outpoints, keys, index] = schema::validate (query, import_options_pattern);

        Txs = txs;
        Txs = Txs + read_body (content_type, body);

        // check that we have at least one of tx, outpoint, or keys.
        if (Txs.size () == 0 && outpoints.size () == 0 && keys.size () == 0)
            throw data::exception {} << "Need at least one of tx, outpoint, or key";

        // if we have index, then we should have no outpoints and exactly one tx.
        if (index) {
            if (outpoints.size () != 0 || Txs.size () != 1)
                throw data::exception {} << "If 'index' is given, no 'outpoint' is allowed and only one 'tx' is allowed.";

            Outpoints = {Bitcoin::outpoint {Txs.first ().visit ([] (const auto &n) {
                if constexpr (std::is_same_v<std::decay_t<decltype (n)>, Bitcoin::TxID>) {
                    return n;
                } else if constexpr (std::is_same_v<std::decay_t<decltype (n)>, BEEF>) {
                    return data::last (n.Transactions).id ();
                } else {
                    return n.id ();
                }
            }), *index}};
        } else Outpoints = outpoints;

        Keys = Keys;
    }
}
