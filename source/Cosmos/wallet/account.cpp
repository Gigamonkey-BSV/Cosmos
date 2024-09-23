
#include <Cosmos/wallet/account.hpp>

namespace Cosmos {

    account account::operator << (const account_diff &d) const {
        account a = *this;
        for (const auto &o : d.Remove) {
            const auto *x = a.contains (o);
            if (!bool (x)) throw exception {} << "invalid account for diff";
            a = a.remove (o);
        }

        for (const auto &e: d.Insert) a = a.insert (Bitcoin::outpoint {d.TXID, e.Key}, e.Value);
        return a;
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

        if (auto xx = j.find ("script_code"); xx != j.end ()) {
            auto x = encoding::hex::read (std::string (*xx));
            if (!bool (x)) throw exception {} << "could not read hex value from \"" + xx->dump () + "\"";
            UnlockScriptSoFar = *x;
        }

    }

    account::account (const JSON &j) {

        if (j == nullptr) return;

        if (!j.is_object ()) throw exception {} << "invalid account JSON format 1";

        account a {};
        for (const auto &[key, value] : j.items ()) a = a.insert (read_outpoint (key), redeemable {value});

        *this = a;
    }

    account::operator JSON () const {
        JSON::object_t a;
        for (const auto &[key, value] : *this) a[write (key)] = JSON (value);

        return a;
    }

}

