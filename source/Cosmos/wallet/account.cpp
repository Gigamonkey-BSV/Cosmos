
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
        if (!j.is_object ()) throw exception {} << "invalid account JSON format";

        for (const auto &[key, value] : j.items ()) Account [read_outpoint (key)] = redeemable {value};
        return;
    }

    account::operator JSON () const {
        JSON::object_t a;

        for (const auto &[key, value] : Account) a[write (key)] = JSON (value);

        return a;
    }

    ray read_ray (const JSON &j, const Bitcoin::timestamp &when) {
        if (j.contains ("output")) return ray {when, static_cast<uint64> (int64 (j["index"])),
            read_outpoint (j["outpoint"]), read_output (j["output"])};
        return ray {when, static_cast<uint64> (int64 (j["index"])), inpoint {read_outpoint (j["inpoint"])},
            read_input (j["input"]), Bitcoin::satoshi {int64 (j["value"])}};
    }

    JSON write_ray (const ray &r) {
        JSON::object_t o;

        if (r.Index > std::numeric_limits<int64>::max ())
            throw exception {} << "index in block is too big to represent as an integer in the JSON library we are using";

        o["index"] = static_cast<int64> (r.Index);
        if (r.Direction == direction::out) {
            o["output"] = write (Bitcoin::output {r.Put});
            o["outpoint"] = write (Bitcoin::outpoint {r.Point});
        } else {
            o["value"] = uint64 (r.Value);
            o["input"] = write (Bitcoin::output {r.Put});
            o["inpoint"] = write (Bitcoin::outpoint {r.Point});
        }
        return o;
    }

    JSON write_event (const events::event &e) {
        JSON::object_t o;
        o["txid"] = write (e.TXID);
        o["when"] = uint32 (e.When);
        o["received"] = int64 (e.Received);
        o["spent"] = int64 (e.Spent);
        o["moved"] = int64 (e.Moved);
        JSON::array_t a;
        a.resize (e.Events.size ());
        int i = 0;
        for (const ray &r : e.Events) a[i++] = write_ray (r);
        o["events"] = a;
        return o;
    }

    events::event read_event (const JSON &j) {
        events::event e {};
        e.TXID = read_txid (std::string (j["txid"]));
        e.When = Bitcoin::timestamp (uint32 (j["when"]));
        e.Received = Bitcoin::satoshi (uint64 (j["received"]));
        e.Spent = Bitcoin::satoshi (uint64 (j["spent"]));
        e.Moved = Bitcoin::satoshi (uint64 (j["moved"]));
        stack<ray> events;
        for (const auto &jj : j["events"]) events <<= read_ray (jj, e.When);
        e.Events = ordered_list<ray> (data::reverse (events));
        return e;
    }

    events::operator JSON () const {
        JSON::object_t ev;

        ev["latest"] = uint32 (Latest);
        ev["value"] = Value;
        ev["spent"] = Spent;
        ev["received"] = Received;

        JSON::array_t events;
        events.resize (Events.size ());
        int i = 0;
        for (const event &e : Events) events[i++] = write_event (e);
        ev["events"] = events;

        JSON::object_t account;
        for (const auto &[key, value] : Account) account[write (key)] = write (value);
        ev["account"] = account;

        return ev;
    }

    events::events (const JSON &j) {
        if (j == JSON (nullptr)) return;

        Latest = Bitcoin::timestamp {uint32 (j["latest"])};
        Value = Bitcoin::satoshi {int64 (j["value"])};
        Spent = Bitcoin::satoshi {int64 (j["spent"])};
        Received = Bitcoin::satoshi {int64 (j["received"])};

        for (const auto &[key, value] : j["account"].items ()) Account[read_outpoint (key)] = read_output (value);

        for (const auto &jj : j["events"]) Events <<= read_event (jj);
    }

    // get all events corresponding to the same tx.
    ordered_list<ray> get_next_tx (ordered_list<ray> &e) {
        if (data::size (e) == 0) return {};

        stack<ray> next_tx;
        const auto &txid = e.first ().Point.Digest;

        while (true) {
            next_tx = next_tx << e.first ();

            e = e.rest ();

            if (data::size (e) == 0 || e.first ().Point.Digest != txid) return reverse (next_tx);
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

            // this is not correct. It is theoretically possible to
            // receive and spend at the same time, although you
            // wouldn't normally do that.
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

    events::history events::get_history (
        math::signed_limit<Bitcoin::timestamp> from,
        math::signed_limit<Bitcoin::timestamp> to) const {

        events ev;
        stack<event> h;
        for (const event &e : Events) if (from > e.When) ev <<= e.Events;
        else if (to >= e.When) h <<= e;

        return history {ev.Account, ordered_list<event> (reverse (h))};
    }

}

