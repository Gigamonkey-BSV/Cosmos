#include <Cosmos/database/json/txdb.hpp>

namespace Cosmos {

    void JSON_local_txdb::add_script (const digest256 &script_hash, const Bitcoin::outpoint &op) {
        auto v = ScriptIndex.find (script_hash);
        if (v != ScriptIndex.end ()) v->second = data::append (v->second, op);
        else ScriptIndex[script_hash] = list<Bitcoin::outpoint> {op};
    }

    void JSON_local_txdb::add_address (const Bitcoin::address &addr, const Bitcoin::outpoint &op) {
        auto v = AddressIndex.find (addr);
        if (v != AddressIndex.end ()) v->second = data::append (v->second, op);
        else AddressIndex[addr] = list<Bitcoin::outpoint> {op};
    }

    JSON_local_txdb::operator JSON () const {

        JSON::object_t addresses;
        for (const auto &[key, value] : this->AddressIndex) {
            JSON::array_t outpoints;
            for (const auto &out : value) outpoints.push_back (write (out));
            addresses[std::string (key)] = outpoints;
        }

        JSON::object_t scripts;
        for (const auto &[key, value] : this->ScriptIndex) {
            JSON::array_t outpoints;
            for (const auto &out : value) outpoints.push_back (write (out));
            scripts[write (key)] = outpoints;
        }

        JSON::object_t redeems;
        for (const auto &[key, value] : this->RedeemIndex) redeems[write (key)] = write (value);

        JSON::object_t o;
        o["spvdb"] = JSON (SPVDB);
        o["addresses"] = addresses;
        o["scripts"] = scripts;
        o["redeems"] = redeems;

        return o;
    }

    JSON_local_txdb::JSON_local_txdb (const JSON &j) {
        if (j == JSON (nullptr)) return;

        if (!j.is_object ()) throw exception {} << "invalid TXDB JSON format: not an object.";
        if (!j.contains ("redeems")) throw exception {} << "invalid TXDB JSON format: missing field 'redeems'. ";
        if (!j.contains ("spvdb") || !j.contains ("addresses") || !j.contains ("scripts"))
            throw exception {} << "invalid TXDB JSON format: " << j;

        const JSON &addresses = j["addresses"];
        const JSON &scripts = j["scripts"];
        const JSON &redeems = j["redeems"];

        if (!addresses.is_object () || !scripts.is_object () || !redeems.is_object ())
            throw exception {} << "invalid TXDB JSON format f: " << j;

        SPVDB = JSON_SPV_database (j["spvdb"]);

        for (const auto &[key, value] : addresses.items ()) {
            list<Bitcoin::outpoint> outpoints;
            for (const auto &k : value) outpoints <<= read_outpoint (std::string (k));
            this->AddressIndex[Bitcoin::address (std::string (key))] = outpoints;
        }

        for (const auto &[key, value] : scripts.items ()) {
            list<Bitcoin::outpoint> outpoints;
            for (const auto &k : value) outpoints <<= read_outpoint (std::string (k));
            this->ScriptIndex[read_txid (key)] = outpoints;
        }

        for (const auto &[key, value] : redeems.items ())
            this->RedeemIndex[read_outpoint (key)] = inpoint {read_outpoint (value)};

    }

    ordered_list<ray> JSON_local_txdb::by_address (const Bitcoin::address &a) {
        auto zz = AddressIndex.find (a);
        if (zz == AddressIndex.end ()) return {};
        ordered_list<ray> n;
        for (const Bitcoin::outpoint &o : zz->second) n = n.insert (ray {(*this) [o.Digest], o});
        return n;
    }

    ordered_list<ray> JSON_local_txdb::by_script_hash (const digest256 &x) {
        auto zz = ScriptIndex.find (x);
        if (zz == ScriptIndex.end ()) return {};
        ordered_list<ray> n;
        for (const Bitcoin::outpoint &o : zz->second) n = n.insert (ray {(*this) [o.Digest], o});
        return n;
    }

    ptr<ray> JSON_local_txdb::redeeming (const Bitcoin::outpoint &o) {
        auto zz = RedeemIndex.find (o);
        if (zz == RedeemIndex.end ()) return {};
        auto p = (*this) [o.Digest];
        if (!p) return {};
        return ptr<ray> {new ray {(*this) [zz->second.Digest], zz->second, p.Transaction->Outputs[o.Index].Value}};
    }

}
