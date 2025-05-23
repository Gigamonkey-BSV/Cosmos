#include <Cosmos/wallet/sign.hpp>

namespace Cosmos::nosig {

    // throws if signatures are not complete.
    input::operator extended::input () const {
        lazy_bytes_writer w;
        for (const auto &x : Script) {
            if (x.is<sigop> ()) throw data::exception {} << "incomplete signature";
            w << x.get<bytes> ();
        }
        return extended::input {Prevout, Bitcoin::input {this->Reference, w.complete (), this->Sequence}};
    }

    // attempt to retrieve all keys and sign.
    transaction transaction::sign (database::readable &db) const {
        Bitcoin::incomplete::transaction incomplete (*this);
        return transaction {Version, data::for_each ([&] (const input &in) -> input {
            return input {in.Prevout.Value, in.Prevout.Script, in.Reference,
                data::for_each ([&] (const element &x) -> element {
                    if (x.is<sigop> ()) {
                        auto op = x.get<sigop> ();
                        auto sig = db.sign (incomplete, op.Key);
                        if (!bool (sig)) return x;
                        else return data::bytes (Bitcoin::signature {*sig, op.Directive});
                    } else return x;
                }, in.Script), in.Sequence};
        }, Inputs), Outputs, LockTime};
    }

    // throws if signatures are not complete.
    transaction::operator extended::transaction () const {
        return extended_transaction {Version, data::for_each ([] (const input &in) -> extended::input {
            return extended::input (in);
        }, Inputs), Outputs, LockTime};
    }

}
