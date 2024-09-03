
#include <Cosmos/database/json/spv.hpp>
#include <Cosmos/database/write.hpp>
#include <data/encoding/base64.hpp>

namespace Cosmos {

    JSON write (const SPV::database::memory::entry &e) {
        JSON::object_t o;
        o["header"] = Cosmos::write (e.Header.Value);
        o["tree"] = JSON (e.BUMP ());
        return o;
    }

    ptr<SPV::database::memory::entry> read_db_entry (const JSON &j) {
        Merkle::BUMP bump (j["tree"]);
        return std::make_shared<SPV::database::memory::entry> (N (bump.BlockHeight), read_header (std::string (j["header"])), bump.paths ());
    }

    JSON_SPV_database::operator JSON () const {
        JSON::array_t by_height;
        by_height.resize (this->ByHeight.size ());
        int ind = 0;
        for (const auto &[height, entry] : this->ByHeight)
            by_height[ind] = write (*entry);

        JSON::object_t by_hash;
        for (const auto &[hash, entry] : this->ByHash)
            by_hash[write (hash)] = write (entry->Header.Key);

        JSON::object_t by_root;
        for (const auto &[root, entry] : this->ByRoot)
            by_hash[write (root)] = write (entry->Header.Key);

        JSON::object_t txs;
        for (const auto &[txid, tx] : this->Transactions)
            txs[write (txid)] = encoding::base64::write (bytes (*tx));

        JSON::object_t o;
        o["by_height"] = by_height;
        o["by_hash"] = by_hash;
        o["by_root"] = by_root;
        o["txs"] = txs;
        return o;
    }

    JSON_SPV_database::JSON_SPV_database (const JSON &j) {

        ptr<SPV::database::memory::entry> last {nullptr};
        for (const auto &jj : j["by_height"]) {
            ptr<SPV::database::memory::entry> e = read_db_entry (jj);
            if (last != nullptr && last->Header.Key + 1 == e->Header.Key) e->Last = last;
            last = e;
            ByHeight[e->Header.Key] = e;
            for (const auto &d: e->Paths.keys ()) ByTXID[d] = e;
        }

        Latest = last;

        for (const auto &[hash, height] : j["by_hash"].items ())
            ByHash[read_txid (hash)] = ByHeight [read_N (height)];

        for (const auto &[root, height] : j["by_root"].items ())
            ByHash[read_txid (root)] = ByHeight [read_N (height)];

        for (const auto &[txid, tx] : j["txs"].items ())
            Transactions[read_txid (txid)] = ptr<Bitcoin::transaction>
                {new Bitcoin::transaction {*encoding::base64::read (std::string (tx))}};

    }
}

