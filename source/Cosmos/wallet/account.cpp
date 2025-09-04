
#include <Cosmos/wallet/account.hpp>
#include <Cosmos/database/write.hpp>

namespace Cosmos {

    account account::operator << (const account_diff &d) const {
        account a = *this;
        for (const auto &[_, o] : d.Remove) {
            const auto *x = a.contains (o);
            if (!bool (x)) throw cannot_apply_diff {};
            a = a.remove (o);
        }

        for (const auto &e: d.Insert) a = a.insert (Bitcoin::outpoint {d.TXID, e.Key}, e.Value);
        return a;
    }

    redeemable::operator JSON () const {
        JSON::array_t deriv;

        {
            int index = 0;
            deriv.resize (Keys.size ());
            for (const key_expression &k : Keys) deriv[index++] = k;
        }

        JSON::object_t j;
        j["keys"] = deriv;
        if (UnlockScriptSoFar.size () != 0) j["script_code"] = encoding::hex::write (UnlockScriptSoFar);
        j["expected_size"] = ExpectedScriptSize;
        j["prevout"] = write (Prevout);
        return j;
    }

    // throw if failure.
    key_expression read_derivation (const JSON &) {
        throw data::method::unimplemented {"read_derivation"};
    }

    redeemable::redeemable (const JSON &j) : signing {} {

        this->ExpectedScriptSize = uint32 (j["expected_size"]);

        Prevout = read_output (j["prevout"]);

        // this is an old format. Derivation was a key and a bip32 path.
        if (j.contains ("derivation")) {

            const JSON &p = j["derivation"];

            if (!p.is_array ()) throw data::exception {} << "invalid redeemable format";

            for (const JSON &bd : p) Keys <<= read_derivation (bd);
        } else if (j.contains ("keys")) {
            const JSON &p = j["keys"];

            if (!p.is_array ()) throw data::exception {} << "invalid redeemable format";

            for (const JSON &k : p) Keys <<= key_expression (std::string (k));
        }

        if (auto xx = j.find ("script_code"); xx != j.end ()) {
            auto x = encoding::hex::read (std::string (*xx));
            if (!bool (x)) throw data::exception {} << "could not read hex value from \"" + xx->dump () + "\"";
            UnlockScriptSoFar = *x;
        }

    }

    account::account (const JSON &j) {

        if (j == nullptr) return;

        if (!j.is_object ()) throw data::exception {} << "invalid account JSON format 1";

        account a {};
        for (const auto &[key, value] : j.items ()) a = a.insert (read_outpoint (key), redeemable {value});

        *this = a;
    }

    account::operator JSON () const {
        JSON::object_t a;
        for (const auto &[key, value] : *this) a[Cosmos::write (key)] = JSON (value);

        return a;
    }

    account::account_details account::details () const {
        account_details d;
        for (const auto &[key, value] : *this) {
            d.Value += value.Prevout.Value;
            if (value.Prevout.Value > d.Max.value ())
                d.Max = {key, value.Prevout};
            d.MeanValue = double (d.Value) / double (this->size ());
        };
        return d;
    }

    account::account_details::operator JSON () const {
        return JSON::object_t {
            {"value", int64 (Value)},
            {"max", Cosmos::write (Max)},
            {"mean", MeanValue}};
    }

}

