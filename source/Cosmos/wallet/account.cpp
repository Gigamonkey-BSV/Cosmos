
#include <Cosmos/wallet/account.hpp>

namespace Cosmos {

    redeemable::operator JSON () const {
        JSON::object_t j;
        JSON::array_t deriv;
        for (const derivation &d : Derivation) deriv.push_back (JSON (d));
        j["derivation"] = deriv;
        if (UnlockScriptSoFar.size () != 0) j["script_code"] = encoding::hex::write (UnlockScriptSoFar);
        j["expected_size"] = ExpectedScriptSize;
        j["prevout"] = write (Prevout);
        return j;
    }

    redeemable::redeemable (const JSON &j) : signing {derivation {}, uint32 (j["expected_size"])} {
        Prevout = read_output (j["prevout"]);
        const JSON &p = j["derivation"];
        for (const JSON &d : p) Derivation <<= derivation {d};
        if (j.contains ("script_code")) {
            auto x = encoding::hex::read (std::string (p["script_code"]));
            if (!bool (x)) throw exception {} << "could not read hex value from \"" + p["script"].dump () + "\"";
            UnlockScriptSoFar = *x;
        }
    }

    account::account (const JSON &j) {
        if (j == nullptr) return;

        for (const auto &[key, value] : j.items ()) Account [read_outpoint (key)] = redeemable {value};
        return;
    }

    account::operator JSON () const {
        JSON::object_t a;

        for (const auto &[key, value] : Account) a[write (key)] = JSON (value);

        return a;
    }

    list<ray> get_next_tx (ordered_list<ray> &e) {
        list<ray> next_tx;
        if (data::size (e) == 0) return next_tx;

        const auto &txid = e.first ().Point.Digest;

        while (true) {
            next_tx = next_tx << e.first ();

            e = e.rest ();

            if (data::size (e) == 0) return next_tx;

            if (e.first ().Point.Digest != txid) return next_tx;
        }
    }

    events &events::operator <<= (ordered_list<ray> e) {
        if (data::size (e) == 0) return *this;

        if (e.first ().When <= Latest) throw exception {} << "must be later than latest time";

        while (true) {
            const auto &next = e.first ();

            event next_event {};
            next_event.Events = get_next_tx (e);

            if (data::size (next_event.Events) == 0) return *this;

            next_event.TXID = next.Point.Digest;
            next_event.When = next.When;

            this->Latest = next_event.When;

            Bitcoin::satoshi received = 0;
            Bitcoin::satoshi spent = 0;

            for (const ray &current : next_event.Events) if (current.Direction == direction::in) {
                // delete the output from the account
                this->Account.erase (Bitcoin::input {current.Put}.Reference);

                spent += current.Value;
            } else {
                Bitcoin::output output = Bitcoin::output {current.Put};
                this->Account[current.Point] = output;

                received += current.Value;
            }

            if (received > spent) {
                next_event.Spent = 0;
                next_event.Moved = spent;
                next_event.Received = received - spent;
            } else {
                next_event.Received = 0;
                next_event.Moved = received;
                next_event.Spent = spent - received;
            }

            Events <<= next_event;

            Received += next_event.Received;
            Spent += next_event.Spent;
            Value += next_event.Received;
            Value -= next_event.Spent;

        }

    }

}

