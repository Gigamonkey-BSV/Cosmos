
#include <Cosmos/history.hpp>

namespace Cosmos {
    namespace {

        JSON inline write_direction (direction d) {
            return d == direction::in ? "in" : "out";
        }

        direction read_direction (const JSON &j) {
            if (j.is_string ()) {
                if (std::string (j) == "in") return direction::in;
                if (std::string (j) == "out") return direction::out;
            }

            throw exception {} << "invalid history direction format";
        }

        JSON write_event (const event &r) {
            JSON::object_t o;
            o["point"] = write (Bitcoin::outpoint {r.id (), r.Index});
            o["direction"] = write_direction (r.Direction);
            return o;
        }

        event read_event (const JSON &j, TXDB &txdb) {

            Bitcoin::outpoint point;
            direction dir;

            auto d = j.find ("direction");
            auto p = j.find ("point");

            if (d != j.end () && p != j.end ()) {

                if (!p->is_string ())
                    throw exception {} << "invalid JSON history event format";

                point = read_outpoint (std::string (*p));
                dir = read_direction (std::string (*d));

            } else {

                // these are an old format. If any of these fields are present
                // we need to translate the old format into the new one.

                auto index = j.find ("index");
                if (index == j.end ()) throw exception {} << "invalid ray format JSON: index";

                auto output = j.find ("output");

                if (output != j.end ()) {
                    point = read_outpoint (j["outpoint"]);
                    dir = direction::out;
                } else {
                    point = read_outpoint (j["inpoint"]);
                    dir = direction::in;
                }
            }

            return event {txdb[point.Digest], point.Index, dir};
        }

        JSON write_tx (const history::tx &e) {

            JSON::object_t o;
            o["txid"] = write (e.TXID);
            o["when"] = JSON (e.When);
            o["received"] = write (e.Received);
            o["spent"] = write (e.Spent);
            o["moved"] = write (e.Moved);
            JSON::array_t a;
            a.resize (e.Events.size ());
            int i = 0;
            for (const event &r : e.Events) a[i++] = write_event (r);
            o["events"] = a;
            return o;

        }

        history::tx read_tx (const JSON &j, TXDB &txdb) {

            if (!j.is_object ()) throw exception {} << "invalid event JSON format";
            auto txid = j.find ("txid");
            auto time = j.find ("when");
            auto received = j.find ("received");
            auto spent = j.find ("spent");
            auto moved = j.find ("moved");
            auto events = j.find ("events");

            if (txid == j.end ()) throw exception {} << "invalid event JSON format: 'txid'";
            if (time == j.end ()) throw exception {} << "invalid event JSON format: 'when'";
            if (received == j.end ()) throw exception {} << "invalid event JSON format: 'received'";
            if (spent == j.end ()) throw exception {} << "invalid event JSON format: 'spent'";
            if (moved == j.end ()) throw exception {} << "invalid event JSON format: 'moved'";
            if (events == j.end () || !events->is_array ()) throw exception {} << "invalid event JSON format: 'events'";

            history::tx e {};
            e.TXID = read_TXID (std::string (*txid));
            e.When = when (*time);
            e.Received = read_satoshi (*received);
            e.Spent = read_satoshi (*spent);
            e.Moved = read_satoshi (*moved);
            stack<event> evv;
            for (const auto &jj : *events) evv >>= read_event (jj, txdb);
            e.Events = ordered_sequence<event> (reverse (evv));
            return e;

        }

        history::payment read_payment (const JSON &j) {
            payments::payment_request pr = payments::read_payment_request (j);
            list<Bitcoin::TXID> txs;
            for (const auto &tt : j["txs"]) txs <<= read_TXID (std::string (tt));
            return history::payment {pr.Key, pr.Value, txs};
        }

        JSON write_payment (const history::payment &p) {
            JSON pj = payments::write_payment_request (p);
            JSON::array_t txs;
            txs.resize (p.Transactions.size ());
            uint32 index = 0;
            for (const auto &txid : p.Transactions) txs[index++] = write (txid);
            pj["txs"] = txs;
            return pj;
        }

    }

    history::operator JSON () const {
        JSON::object_t ev;

        ev["value"] = write (Value);
        ev["spent"] = write (Spent);
        ev["received"] = write (Received);

        JSON::array_t events;
        events.resize (Events.size ());
        int i = Events.size () - 1;
        for (const tx &e : Events) events[i--] = write_tx (e);
        ev["events"] = events;

        JSON::object_t account;
        for (const auto &[key, value] : Account) account[write (key)] = write (value);
        ev["account"] = account;

        JSON::array_t payments;
        i = Events.size () - 1;
        for (const payment &p : Payments) payments[i--] = write_payment (p);
        ev["payments"] = payments;

        return ev;
    }

    history::history (const JSON &j, TXDB &txdb) {
        if (j == JSON (nullptr)) return;

        if (!j.is_object ()) throw exception {} << "invalid events JSON format";

        // this was an old format. If it is present it can be ignored now.
        // it might take a long time to read in the old format because it
        // will be read in the wrong order, but it will all work.
        //auto latest = j.find ("latest");
        auto value = j.find ("value");
        auto spent = j.find ("spent");
        auto received = j.find ("received");
        auto account = j.find ("account");
        auto events = j.find ("events");

        if (value == j.end () || spent == j.end () || received == j.end () || account == j.end () || events == j.end ())
            throw exception {} << "invalid events JSON format: missing fields";

        //if (!latest->is_number ()) throw exception {} << "invalid events JSON format: invalid field latest";
        if (!value->is_number ()) throw exception {} << "invalid events JSON format: invalid field value";
        if (!spent->is_number ()) throw exception {} << "invalid events JSON format: invalid field spent";
        if (!received->is_number ()) throw exception {} << "invalid events JSON format: invalid field received";

        Value = read_satoshi (*value);
        Spent = read_satoshi (*spent);
        Received = read_satoshi (*received);

        for (const auto &[key, value] : j["account"].items ()) Account[read_outpoint (key)] = read_output (value);

        for (const auto &jj : j["events"]) Events >>= read_tx (jj, txdb);

        // this is a new feature.
        auto payments = j.find ("payment");
        if (payments != j.end ()) for (const auto &jj : *payments) Payments >>= read_payment (jj);

    }

    // get all events corresponding to the same tx.
    events get_next_tx (events &e) {
        if (data::size (e) == 0) return {};

        stack<event> next_tx;
        const auto &txid = e.first ().id ();

        while (true) {
            next_tx >>= first (e);

            e = rest (e);

            if (data::size (e) == 0 || first (e).id () != txid) return reverse (next_tx);
        }
    }

    history &history::operator <<= (events e) {
        if (data::empty (e)) return *this;

        if (!data::empty (Events) && first (e) < first (first (Events).Events))
            throw exception {} << "must be later than latest event";

        while (true) {
            const auto &next = e.first ();

            tx next_event {};
            next_event.Events = get_next_tx (e);

            if (data::size (next_event.Events) == 0) return *this;

            next_event.TXID = next.id ();
            next_event.When = next->when ();

            Bitcoin::satoshi received = 0;
            Bitcoin::satoshi spent = 0;

            for (const event &current : next_event.Events) if (current.Direction == direction::in) {
                // delete the output from the account
                this->Account.erase (Bitcoin::input {current.put ()}.Reference);

                spent += current.value ();
            } else {
                Bitcoin::output output = Bitcoin::output {current.put ()};
                this->Account[current.point ()] = output;

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

            Events >>= next_event;

            Received += next_event.Received;
            Spent += next_event.Spent;
            Value += next_event.Received;
            Value -= next_event.Spent;

        }

    }

    history::episode history::get (when from, when to) const {

        when latest = latest_known ();
        if (to > latest) to = when::infinity ();

        history ev;
        stack<tx> h;
        for (const tx &e : reverse (Events)) {
            when w = e.When;
            if (w < from) ev <<= e.Events;
            else if (w < to) h >>= e;
        }

        return episode {ev.Account, ordered_sequence<tx> (reverse (h))};
    }

    // the latest known timestamp before unconfirmed events.
    when history::latest_known () const {
        auto e = Events;
        while (!e.empty ()) {
            if (first (e).When != when::unconfirmed ()) return first (e).When;
            e = rest (e);
        }
        return when::negative_infinity ();
    }
}
