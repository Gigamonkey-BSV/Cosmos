
#include <Cosmos/wallet/account.hpp>

namespace Cosmos {

    account &account::operator <<= (const account_diff &d) {
        for (const auto &o : d.Remove) {
            auto x = Account.find (o);
            if (x == Account.end ()) throw exception {} << "invalid account for diff";
            Account.erase (x);
        }

        for (const auto &e: d.Insert) Account[Bitcoin::outpoint {d.TXID, e.Key}] = e.Value;
        return *this;
    }

    redeemable::operator JSON () const {
        JSON::array_t deriv;

        {
            int index = 0;
            deriv.resize (Derivation.size ());
            for (const derivation &d : Derivation) deriv[index++] = JSON (d);
        }

        JSON::object_t j;
        j["derivation"] = deriv;
        if (UnlockScriptSoFar.size () != 0) j["script_code"] = encoding::hex::write (UnlockScriptSoFar);
        j["expected_size"] = ExpectedScriptSize;
        j["prevout"] = write (Prevout);
        return j;
    }

    redeemable::redeemable (const JSON &j) : signing {} {

        this->ExpectedScriptSize = uint32 (j["expected_size"]);

        Prevout = read_output (j["prevout"]);
        const JSON &p = j["derivation"];

        if (!p.is_array ()) throw exception {} << "invalid redeemable format";

        auto bd = p.begin ();

        while (bd != p.end ()) {
            auto dd = derivation {*bd};
            if (!dd.valid ()) throw exception {} << "invalid derivation";
            Derivation <<= dd;
            bd++;
        }

        if (j.contains ("script_code")) {
            auto x = encoding::hex::read (std::string (p["script_code"]));
            if (!bool (x)) throw exception {} << "could not read hex value from \"" + p["script"].dump () + "\"";
            UnlockScriptSoFar = *x;
        }

    }

    account::account (const JSON &j) {

        if (j == nullptr) return;

        if (!j.is_object ()) throw exception {} << "invalid account JSON format 1";

        for (const auto &[key, value] : j.items ()) Account [read_outpoint (key)] = redeemable {value};

    }

    account::operator JSON () const {
        JSON::object_t a;
        for (const auto &[key, value] : Account) a[write (key)] = JSON (value);

        return a;
    }

    ray read_ray (const JSON &j, const Bitcoin::timestamp &when) {

        auto index = j.find ("index");
        if (index == j.end ()) throw exception {} << "invalid ray format JSON: index";

        auto output = j.find ("output");

        if (output != j.end ()) {
            return ray {when,
                static_cast<uint64> (int64 (j["index"])),
                read_outpoint (j["outpoint"]),
                read_output (*output)};
        }

        auto inpoint = j.find ("inpoint");
        auto input = j.find ("input");
        auto value = j.find ("value");

        if (inpoint == j.end ()) throw exception {} << "invalid ray format JSON: inpoint";
        if (input == j.end ()) throw exception {} << "invalid ray format JSON: input";
        if (value == j.end ()) throw exception {} << "invalid ray format JSON: value";

        return ray {when,
            static_cast<uint64> (int64 (j["index"])),
            Cosmos::inpoint {read_outpoint (j["inpoint"])},
            read_input (j["input"]),
            read_satoshi (j["value"])};
    }

    JSON write_ray (const ray &r) {
        JSON::object_t o;

        if (r.Index > std::numeric_limits<int64>::max ())
            throw exception {} << "index in block is too big to represent as an integer in the JSON library we are using";

        o["index"] = static_cast<int64> (r.Index);
        if (r.direction () == direction::out) {
            o["output"] = write (Bitcoin::output {r.Put});
            o["outpoint"] = write (Bitcoin::outpoint {r.Point});
        } else {
            o["input"] = write (Bitcoin::input {r.Put});
            o["inpoint"] = write (Bitcoin::outpoint {r.Point});
            o["value"] = write (r.value ());
        }
        return o;
    }

    JSON write_event (const events::event &e) {

        JSON::object_t o;
        o["txid"] = write (e.TXID);
        o["when"] = uint32 (e.When);
        o["received"] = write (e.Received);
        o["spent"] = write (e.Spent);
        o["moved"] = write (e.Moved);
        JSON::array_t a;
        a.resize (e.Events.size ());
        int i = 0;
        for (const ray &r : e.Events) a[i++] = write_ray (r);
        o["events"] = a;
        return o;

    }

    events::event read_event (const JSON &j) {

        if (!j.is_object ()) throw exception {} << "invalid event JSON format";
        auto txid = j.find ("txid");
        auto when = j.find ("when");
        auto received = j.find ("received");
        auto spent = j.find ("spent");
        auto moved = j.find ("moved");
        auto events = j.find ("events");

        if (txid == j.end ()) throw exception {} << "invalid event JSON format: 'txid'";
        if (when == j.end () || !when->is_number ()) throw exception {} << "invalid event JSON format: 'when'";
        if (received == j.end ()) throw exception {} << "invalid event JSON format: 'received'";
        if (spent == j.end ()) throw exception {} << "invalid event JSON format: 'spent'";
        if (moved == j.end ()) throw exception {} << "invalid event JSON format: 'moved'";
        if (events == j.end () || !events->is_array ()) throw exception {} << "invalid event JSON format: 'events'";

        events::event e {};
        e.TXID = read_txid (std::string (*txid));
        e.When = Bitcoin::timestamp (uint32 (*when));
        e.Received = read_satoshi (*received);
        e.Spent = read_satoshi (*spent);
        e.Moved = read_satoshi (*moved);
        stack<ray> evv;
        for (const auto &jj : *events) evv <<= read_ray (jj, e.When);
        e.Events = ordered_list<ray> (data::reverse (evv));
        return e;

    }

    events::operator JSON () const {
        JSON::object_t ev;

        ev["latest"] = uint32 (Latest);
        ev["value"] = write (Value);
        ev["spent"] = write (Spent);
        ev["received"] = write (Received);

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

        if (!j.is_object ()) throw exception {} << "invalid events JSON format";

        auto latest = j.find ("latest");
        auto value = j.find ("value");
        auto spent = j.find ("spent");
        auto received = j.find ("received");
        auto account = j.find ("account");
        auto events = j.find ("events");

        if (latest == j.end () || value == j.end () || spent == j.end () || received == j.end () || account == j.end () || events == j.end ())
            throw exception {} << "invalid events JSON format: missing fields";

        if (!latest->is_number ()) throw exception {} << "invalid events JSON format: invalid field latest";
        if (!value->is_number ()) throw exception {} << "invalid events JSON format: invalid field value";
        if (!spent->is_number ()) throw exception {} << "invalid events JSON format: invalid field spent";
        if (!received->is_number ()) throw exception {} << "invalid events JSON format: invalid field received";

        Latest = Bitcoin::timestamp {uint32 (*latest)};
        Value = read_satoshi (*value);
        Spent = read_satoshi (*spent);
        Received = read_satoshi (*received);

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

            for (const ray &current : next_event.Events) if (current.direction () == direction::in) {
                // delete the output from the account
                this->Account.erase (Bitcoin::input {current.Put}.Reference);

                spent += current.value ();
            } else {
                Bitcoin::output output = Bitcoin::output {current.Put};
                this->Account[current.Point] = output;

                received += current.value ();
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

