
#include <Cosmos/database/json/txdb.hpp>
#include <Cosmos/database/write.hpp>
#include <data/encoding/base64.hpp>

namespace Cosmos {

    JSON write (const SPV::database::memory::entry &e) {
        JSON::object_t o;
        o["header"] = Cosmos::write (e.Header.Value);
        Merkle::BUMP bump = e.BUMP ();
        if (bump.valid ()) o["tree"] = JSON (bump);
        else o["height"] = uint64 (e.Header.Key);
        return o;
    }

    ptr<SPV::database::memory::entry> read_db_entry (const JSON &j) {
        if (!j.is_object () || !j.contains ("header") || (!j.contains ("height") && !j.contains ("tree")))
            throw exception {} << "invalid SPV DB entry: " << j;
        if (j.contains ("tree")) return std::make_shared<SPV::database::memory::entry>
            (read_header (std::string (j["header"])), Merkle::BUMP {j["tree"]});
        else return std::make_shared<SPV::database::memory::entry>
            (N (uint64 (j["height"])), read_header (std::string (j["header"])));
    }

    JSON_local_TXDB::operator JSON () const {
        JSON::array_t by_height;
        by_height.resize (this->ByHeight.size ());

        int ind = 0;
        for (const auto &[height, entry] : this->ByHeight)
            by_height[ind++] = write (*entry);

        JSON::object_t by_hash;
        for (const auto &[hash, entry] : this->ByHash)
            by_hash[write (hash)] = write (entry->Header.Key);

        JSON::object_t by_root;
        for (const auto &[root, entry] : this->ByRoot)
            by_root[write (root)] = write (entry->Header.Key);

        JSON::object_t txs;
        for (const auto &[txid, tx] : this->Transactions)
            txs[write (txid)] = encoding::base64::write (bytes (*tx));

        JSON::array_t unconfirmed;
        unconfirmed.resize (this->Pending.size ());
        ind = 0;
        for (const auto &txid : this->Pending) unconfirmed[ind++] = write (txid);

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
        o["by_height"] = by_height;
        o["by_hash"] = by_hash;
        o["by_root"] = by_root;
        o["txs"] = txs;
        o["addresses"] = addresses;
        o["scripts"] = scripts;
        o["redeems"] = redeems;
        o["unconfirmed"] = unconfirmed;
        return o;
    }

    void read_SPVDB (memory_local_TXDB &txdb, const JSON &j) {

        if (!j.is_object () || !j.contains ("by_height") || !j.contains ("by_hash") || !j.contains ("by_root") || !j.contains ("txs"))
            throw exception {} << "invalid JSON SPV database format: " << j;

        const auto by_height = j.find ("by_height");
        const auto by_hash = j.find ("by_hash");
        const auto by_root = j.find ("by_root");
        const auto txs = j.find ("txs");

        if (by_height == j.end () || by_hash == j.end () || by_root == j.end () || txs == j.end ())
            throw exception {} << "invalid JSON SPV database format: missing field";

        if (!by_height->is_array () || !by_hash->is_object () || !by_root->is_object () || !txs->is_object ())
            throw exception {} << "invalid JSON SPV database format: invalid field type";

        ptr<SPV::database::memory::entry> last {nullptr};
        for (const auto &jj : *by_height) {
            ptr<SPV::database::memory::entry> e = read_db_entry (jj);
            if (last != nullptr && last->Header.Key + 1 == e->Header.Key) e->Last = last;
            last = e;
            txdb.ByHeight[e->Header.Key] = e;
            for (const auto &d: e->Paths.keys ()) txdb.ByTXID[d] = e;
        }

        txdb.Latest = last;

        for (const auto &[hash, height] : by_hash->items ())
            txdb.ByHash[read_TXID (hash)] = txdb.ByHeight [read_N (height)];

        for (const auto &[root, height] : by_root->items ())
            txdb.ByHash[read_TXID (root)] = txdb.ByHeight [read_N (height)];

        for (const auto &[txid, tx] : txs->items ())
            txdb.Transactions[read_TXID (txid)] = ptr<Bitcoin::transaction> {
                new Bitcoin::transaction {*encoding::base64::read (std::string (tx))}};

        // optional field because I forgot to put it in at one point.
        // In the future it should be mandatory.
        const auto unconfirmed = j.find ("unconfirmed");
        if (unconfirmed != j.end ()) {
            if (!unconfirmed->is_array ()) throw exception {} << "invalid JSON SPV database format: unconfirmed";
            for (const auto &jj : *unconfirmed) txdb.Pending = txdb.Pending.insert (read_TXID (std::string (jj)));
        }

    }

    JSON_local_TXDB::JSON_local_TXDB (const JSON &j) {

        if (j == JSON (nullptr)) return;

        if (!j.is_object ()) throw exception {} << "invalid TXDB JSON format: not an object.";
        if (!j.contains ("redeems")) throw exception {} << "invalid TXDB JSON format: missing field 'redeems'. ";
        if (!j.contains ("addresses") || !j.contains ("scripts"))
            throw exception {} << "invalid TXDB JSON format: ";

        const JSON &addresses = j["addresses"];
        const JSON &scripts = j["scripts"];
        const JSON &redeems = j["redeems"];

        if (!addresses.is_object () || !scripts.is_object () || !redeems.is_object ())
            throw exception {} << "invalid TXDB JSON format ";

        // this is an old format and we would not expect
        // to see this going forward.
        if (j.contains ("spvdb")) read_SPVDB (*this, j["spvdb"]);
        // in the future we will always just do this.
        else read_SPVDB (*this, j);

        for (const auto &[key, value] : addresses.items ()) {
            list<Bitcoin::outpoint> outpoints;
            for (const auto &k : value) outpoints <<= read_outpoint (std::string (k));
            this->AddressIndex[Bitcoin::address (std::string (key))] = outpoints;
        }

        for (const auto &[key, value] : scripts.items ()) {
            list<Bitcoin::outpoint> outpoints;
            for (const auto &k : value) outpoints <<= read_outpoint (std::string (k));
            this->ScriptIndex[read_TXID (key)] = outpoints;
        }

        for (const auto &[key, value] : redeems.items ())
            this->RedeemIndex[read_outpoint (key)] = inpoint {read_outpoint (value)};

    }
}

