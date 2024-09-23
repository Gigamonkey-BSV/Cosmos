
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
        return o;
    }

    void read_SPVDB (memory_local_TXDB &txdb, const JSON &j) {

        if (!j.is_object () || !j.contains ("by_height") || !j.contains ("by_hash") || !j.contains ("by_root") || !j.contains ("txs"))
            throw exception {} << "invalid JSON SPV database format: " << j;

        const JSON &by_height = j["by_height"];
        const JSON &by_hash = j["by_hash"];
        const JSON &by_root = j["by_root"];
        const JSON &txs = j["txs"];

        if (!by_height.is_array () || !by_hash.is_object () || !by_root.is_object () || !txs.is_object ())
            throw exception {} << "invalid JSON SPV database format: " << j;

        ptr<SPV::database::memory::entry> last {nullptr};
        for (const auto &jj : by_height) {
            ptr<SPV::database::memory::entry> e = read_db_entry (jj);
            if (last != nullptr && last->Header.Key + 1 == e->Header.Key) e->Last = last;
            last = e;
            txdb.ByHeight[e->Header.Key] = e;
            for (const auto &d: e->Paths.keys ()) txdb.ByTXID[d] = e;
        }

        txdb.Latest = last;

        for (const auto &[hash, height] : j["by_hash"].items ())
            txdb.ByHash[read_txid (hash)] = txdb.ByHeight [read_N (height)];

        for (const auto &[root, height] : j["by_root"].items ())
            txdb.ByHash[read_txid (root)] = txdb.ByHeight [read_N (height)];

        for (const auto &[txid, tx] : j["txs"].items ())
            txdb.Transactions[read_txid (txid)] = ptr<Bitcoin::transaction>
                {new Bitcoin::transaction {*encoding::base64::read (std::string (tx))}};
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
            this->ScriptIndex[read_txid (key)] = outpoints;
        }

        for (const auto &[key, value] : redeems.items ())
            this->RedeemIndex[read_outpoint (key)] = inpoint {read_outpoint (value)};

    }
}

