#include <Cosmos/database/SQLite/SQLite.hpp>
#include <data/maybe.hpp>

#include <gigamonkey/p2p/net_address.hpp>

#include <sqlite_orm/sqlite_orm.h>
#include <sqlite3.h>

namespace sqlite_orm {
    namespace Bitcoin = Gigamonkey::Bitcoin;

    template <> struct type_printer<std::vector<data::byte>> {
        static std::string print () {
            return "BLOB";
        }
    };

    template <> struct statement_binder<std::vector<data::byte>> {
        int bind (sqlite3_stmt *stmt, int index, const std::vector<data::byte> &value) const {
            return sqlite3_bind_blob (stmt, index, value.data (), static_cast<int> (value.size ()), SQLITE_TRANSIENT);
        }
    };

    template <> struct field_printer<std::vector<data::byte>> {
        static void print (std::ostream &os, const std::vector<data::byte> &value) {
            os << "<blob of size " << value.size () << ">";
        }
    };

    template <> struct row_extractor<std::vector<data::byte>> {
        std::vector<data::byte> extract (sqlite3_stmt* stmt, int columnIndex) const {
            const void* data = sqlite3_column_blob (stmt, columnIndex);
            int size = sqlite3_column_bytes (stmt, columnIndex);
            if (!data || size <= 0) return {};
            const data::byte *bytes = static_cast<const data::byte *> (data);
            return std::vector<data::byte> (bytes, bytes + size);
        }
    };

    template <> struct type_printer<data::bytes> {
        static std::string print () {
            return "BLOB";
        }
    };

    template <> struct statement_binder<data::bytes> {
        int bind (sqlite3_stmt *stmt, int index, const std::vector<data::byte> &value) const {
            return sqlite3_bind_blob (stmt, index, value.data (), static_cast<int> (value.size ()), SQLITE_TRANSIENT);
        }
    };

    template <> struct field_printer<data::bytes> {
        static void print (std::ostream &os, const std::vector<data::byte> &value) {
            os << "<blob of size " << value.size () << ">";
        }

        std::string operator () (const data::bytes &value) const {
            std::stringstream ss;
            ss << value;
            return ss.str ();
        }
    };

    template <> struct row_extractor<data::bytes> {
        data::bytes extract (sqlite3_stmt* stmt, int columnIndex) const {
            const void* data = sqlite3_column_blob (stmt, columnIndex);
            int size = sqlite3_column_bytes (stmt, columnIndex);
            if (!data || size <= 0) return {};
            data::bytes b {};
            b.resize (size);
            const data::byte *bytes = static_cast<const data::byte *> (data);
            std::copy (bytes, bytes + size, b.begin ());
            return b;
        }
    };

    template <size_t size>
    struct type_printer<data::byte_array<size>> {
        static std::string print () {
            return "BLOB";
        }
    };

    template <size_t size>
    struct statement_binder<data::byte_array<size>> {
        int bind (sqlite3_stmt *stmt, int index, const data::byte_array<size> &value) const {
            return sqlite3_bind_blob (stmt, index, value.data (), static_cast<int> (value.size ()), SQLITE_TRANSIENT);
        }
    };

    template <size_t size>
    struct field_printer<data::byte_array<size>> {
        static void print (std::ostream &os, const data::byte_array<size> &value) {
            os << "<blob of size " << value.size () << ">";
        }

        std::string operator () (const data::byte_array<size> &value) const {
            std::stringstream ss;
            ss << value;
            return ss.str ();
        }
    };

    template <size_t size>
    struct row_extractor<data::byte_array<size>> {
        data::byte_array<size> extract (sqlite3_stmt *stmt, int columnIndex) const {
            const void* data = sqlite3_column_blob (stmt, columnIndex);
            if (!data || size <= 0) return {};
            data::byte_array<size> b {};
            const data::byte *bytes = static_cast<const data::byte *> (data);
            std::copy (bytes, bytes + size, b.begin ());
            return b;
        }
    };

    template <size_t size>
    struct type_printer<data::crypto::digest<size>> {
        static std::string print () {
            return "TEXT";
        }
    };

    template <size_t size>
    struct statement_binder<data::crypto::digest<size>> {
        int bind (sqlite3_stmt *stmt, int index, const data::crypto::digest<size> &value) const {
            return sqlite3_bind_text (stmt, index, data::encoding::hex::write (value).c_str (), static_cast<int> (value.size () * 2), SQLITE_TRANSIENT);
        }
    };

    template <size_t size>
    struct field_printer<data::crypto::digest<size>> {
        static void print (std::ostream &os, const data::crypto::digest<size> &value) {
            os << "<text of size " << (value.size () * 2) << ">";
        }

        std::string operator () (const data::crypto::digest<size>& value) const {
            std::stringstream ss;
            ss << value;
            return ss.str ();
        }
    };

    template <size_t size>
    struct row_extractor<data::crypto::digest<size>> {
        data::crypto::digest<size> extract (sqlite3_stmt *stmt, int columnIndex) const {
            std::string hex_text = std::string {reinterpret_cast<const char*> (sqlite3_column_text (stmt, columnIndex))};
            return data::crypto::digest<size> {hex_text};
        }
    };

    template <> struct type_printer<Bitcoin::outpoint> {
        static std::string print () {
            return "TEXT";
        }
    };

    template <> struct statement_binder<Bitcoin::outpoint> {
        int bind (sqlite3_stmt *stmt, int index, const Bitcoin::outpoint &value) const {
            return sqlite3_bind_text (stmt, index,
                data::encoding::hex::write (value.write ()).c_str (),
                static_cast<int> (72), SQLITE_TRANSIENT);
        }
    };

    template <> struct field_printer<Bitcoin::outpoint> {
        static void print (std::ostream &os, const Bitcoin::outpoint &value) {
            os << "<text of size 72>";
        }

        std::string operator () (const Bitcoin::outpoint &value) const {
            std::stringstream ss;
            ss << value;
            return ss.str ();
        }
    };

    template <> struct row_extractor<Bitcoin::outpoint> {
        Bitcoin::outpoint extract (sqlite3_stmt *stmt, int columnIndex) const {
            std::string hex_text = std::string {reinterpret_cast<const char*> (sqlite3_column_text (stmt, columnIndex))};
            data::bytes read = *data::encoding::hex::read (hex_text);
            return Bitcoin::outpoint {data::slice<data::byte, 36> {read.data ()}};
        }
    };

    template <> struct type_printer<Cosmos::inpoint> {
        static std::string print () {
            return "BLOB";
        }
    };

    template <> struct statement_binder<Cosmos::inpoint> {
        int bind (sqlite3_stmt *stmt, int index, const Cosmos::inpoint &value) const {
            return sqlite3_bind_blob (stmt, index, value.write ().data (), static_cast<int> (36), SQLITE_TRANSIENT);
        }
    };

    template <> struct field_printer<Cosmos::inpoint> {
        static void print (std::ostream &os, const Cosmos::inpoint &value) {
            os << "<blob of size 36>";
        }

        std::string operator () (const Cosmos::inpoint &value) const {
            std::stringstream ss;
            ss << value;
            return ss.str ();
        }
    };

    template <> struct row_extractor<Cosmos::inpoint> {
        Cosmos::inpoint extract (sqlite3_stmt *stmt, int columnIndex) const {
            const void* data = sqlite3_column_blob (stmt, columnIndex);
            data::byte_array<36> b {};
            const data::byte *bytes = static_cast<const data::byte *> (data);
            std::copy (bytes, bytes + 36, b.begin ());
            return Cosmos::inpoint {b};
        }
    };

    template <> struct type_printer<Bitcoin::address> {
        static std::string print () {
            return "TEXT";
        }
    };

    template <> struct statement_binder<Bitcoin::address> {
        int bind (sqlite3_stmt *stmt, int index, const Bitcoin::address &value) const {
            return sqlite3_bind_text (stmt, index, value.c_str (),
                static_cast<int> (value.size ()), SQLITE_TRANSIENT);
        }
    };

    template <> struct field_printer<Bitcoin::address> {
        static void print (std::ostream &os, const Bitcoin::address &value) {
            os << "<text of size " << value.size () << ">";
        }

        std::string operator () (const Bitcoin::address &value) const {
            std::stringstream ss;
            ss << value;
            return ss.str ();
        }
    };

    template <> struct row_extractor<Bitcoin::address> {
        Bitcoin::address extract (sqlite3_stmt *stmt, int columnIndex) const {
            return Bitcoin::address {std::string {reinterpret_cast<const char*> (sqlite3_column_text (stmt, columnIndex))}};
        }
    };
}

namespace Cosmos::SQLite {
    using namespace std;
    using string = std::string;
    using namespace sqlite_orm;
    using data::crypto::digest256;

    // database version.
    struct Version {
        uint64_t version;
        std::string details;
    };

    struct Block {
        digest256 hash;       // 64-char hex string
        uint64_t height;
        digest256 root;       // 64-char hex string
        data::byte_array<80> header;  // 80 bytes
        data::bytes merkle_tree;
    };

    struct Transaction {
        digest256 hash;           // txid, 64-char hex
        data::bytes tx;
        optional<uint32_t> height;
        data::byte status;

        constexpr static const data::byte mined = data::byte (128);
        constexpr static const data::byte pending = data::byte (128 + 32 + 16 + 4 + 1);
    };

    struct Redemption {
        Bitcoin::outpoint outpoint;
        Cosmos::inpoint inpoint;
    };

    struct Script {
        digest256 hash;           // txid, 64-char hex
        data::bytes script;
    };

    struct Output {
        Bitcoin::outpoint outpoint;
        digest256 script_hash;
    };

    // a hash digest and the data.
    struct Digest {
        data::bytes digest;
        data::byte function;
        data::bytes data;
    };

    // an address that is found in a script hash.
    struct Address {
        int id;
        Bitcoin::address address;
        digest256 script_hash;
    };

    // this could be a private key or a shared secret.
    // we interpret it based on the string we read, so
    // it could be WIF, HD, or some encrypted format as
    // long as we can distinguish them.
    struct Secret {
        std::string name;
        std::string key;
    };

    // Define one key as deriving from another via a
    // to_public function application. Thus, key is
    // an expression for a private key.
    struct Pubkey {
        std::string name;
        std::string key;
    };

    // Information on how to redeem an output.
    struct Redeemable {
        Bitcoin::outpoint prevout;
        uint64_t expected_input_script_size;
        data::bytes input_script_so_far;
    };

    // a key required to redeem an output
    // (we may need more than one for a given output)
    struct RedeemKey {
        Bitcoin::outpoint prevout;
        std::string key;
    };

    struct Wallet {
        int id;
        std::string name;
    };

    // an output belonging to a wallet.
    struct WalletOutput {
        Bitcoin::outpoint outpoint;
        std::string wallet_name;
    };

    struct AddressSequence {
        int id;

        std::string wallet_name;

        // typically 'receive' or 'change'
        std::string name;

        int32_t next;

        // given an index, this string
        // defines a function that transforms a key
        // (either public or private) into another key.
        std::string derivation_function;

        // this is an expression that gives
        // us a private key from a master key.
        std::string key_name;
    };

    struct Event {
        int id;
        Bitcoin::TXID tx;
        uint32_t when;
        int64_t received;
        int64_t spent;
        int64_t moved;
    };

    struct Price {
        int id;
        std::string unit;
        int64_t timestamp;
        double price;
    };

    struct Connection {
        Bitcoin::p2p::net_address net_address;
        bool success;
        uint64_t duration;
        uint64_t bytes_received;
        uint64_t bytes_sent;
        int friendship_score;
    };

    inline auto init_storage (const std::string &path) {
        return make_storage (path,

            make_table ("versions",
                make_column ("version", &Version::version, primary_key ()),
                make_column ("details", &Version::details)
            ),

            make_index ("idx_blocks_root", &Block::root),
            make_index ("idx_blocks_hash", &Block::hash),

            // Blocks table
            make_table ("blocks",
                make_column ("height", &Block::height, primary_key ()),
                make_column ("hash", &Block::hash),
                make_column ("root", &Block::root),
                make_column ("header", &Block::header),
                make_column ("merkle_tree", &Block::merkle_tree),
                unique (&Block::height)
            ),

            make_index ("idx_transactions_state", &Transaction::status),

            // Transactions table
            make_table ("transactions",
                make_column ("hash", &Transaction::hash, primary_key ()),
                make_column ("tx", &Transaction::tx),
                make_column ("height", &Transaction::height),
                make_column ("state", &Transaction::status)
            ),

            make_table ("redemptions",
                make_column ("hash", &Redemption::outpoint, primary_key ()),
                make_column ("script", &Redemption::inpoint)
            ),

            make_table ("scripts",
                make_column ("hash", &Script::hash, primary_key ()),
                make_column ("script", &Script::script)
            ),

            make_table ("outputs",
                make_column ("outpoint", &Output::outpoint, primary_key ()),
                make_column ("script_hash", &Output::script_hash)
            ),

            make_table ("digests", 
                make_column ("digest", &Digest::digest, primary_key ()), 
                make_column ("function", &Digest::function), 
                make_column ("data", &Digest::data), 
                unique (&Digest::digest)
            ), 

            make_table ("addresses",
                make_column ("id", &Address::id, primary_key ().autoincrement ()),
                make_column ("address", &Address::address),
                make_column ("script_hash", &Address::script_hash),
                unique (&Address::address, &Address::script_hash)
            ),

            make_table ("secrets",
                make_column ("name", &Secret::name, primary_key ()),
                make_column ("key", &Secret::key)
            ),

            make_table ("pubkeys",
                make_column ("name", &Pubkey::name, primary_key ()),
                make_column ("key", &Pubkey::key)
            ),

            make_table ("redeemables",
                make_column ("id", &Redeemable::prevout, primary_key ()),
                make_column ("expected_input_script_size", &Redeemable::expected_input_script_size),
                make_column ("input_script_so_far", &Redeemable::input_script_so_far)
            ),

            make_table ("redeem_keys",
                make_column ("prevout", &RedeemKey::prevout, primary_key ()),
                make_column ("key", &RedeemKey::key)
            ),

            make_table ("wallets",
                make_column ("id", &Wallet::id, primary_key ().autoincrement ()),
                make_column ("name", &Wallet::name)
            ),

            make_table ("wallet_outputs",
                make_column ("output", &WalletOutput::outpoint, primary_key ()),
                make_column ("wallet_name", &WalletOutput::wallet_name)
            ),

            make_table ("sequences",
                make_column ("id", &AddressSequence::id, primary_key ().autoincrement ()),
                make_column ("wallet_name", &AddressSequence::wallet_name),
                make_column ("name", &AddressSequence::name),
                make_column ("next", &AddressSequence::next),
                make_column ("derivation_function", &AddressSequence::derivation_function),
                make_column ("key_name", &AddressSequence::key_name)
            ),

            make_table ("events",
                make_column ("id", &Event::id, primary_key ().autoincrement ()),
                make_column ("tx", &Event::tx),
                make_column ("when", &Event::when),
                make_column ("received", &Event::received),
                make_column ("spent", &Event::spent),
                make_column ("moved", &Event::moved)
            ),

            make_index ("idx_prices", &Price::unit, &Price::timestamp),

            make_table ("prices",
                make_column ("id", &Price::id, primary_key ().autoincrement ()),
                make_column ("unit", &Price::unit),
                make_column ("timestamp", &Price::timestamp),
                make_column ("price", &Price::price),
                unique (&Price::unit, &Price::timestamp)
            )
        );
    }

    struct db final : database {
        using SPV::database::block_header;
        using SPV::database::tx;

        decltype (init_storage ("")) storage;

        optional<uint64_t> get_latest_version () {
            auto rows = storage.select (
                &Version::version,
                order_by (&Version::version).desc (),
                limit (1));

            if (rows.empty ()) return {};

            return rows.front ();
        }

        db (const std::string &db_path): storage (init_storage (db_path)) {
            storage.sync_schema (true); // Ensures tables exist
            auto opt_latest_version = get_latest_version ();
            if (!bool (opt_latest_version)) storage.insert (Version {2, "First SQLite db"});
            else if (get_latest_version () > 2) throw data::exception {} << "unrecognized database";
        }

        /*
            SPV database
        */

        block_header header (const N &height) final override {
            auto rows = storage.select (
                columns (&Block::header, &Block::hash),
                where (is_equal (&Block::height, uint64_t (height))), limit (1));

            if (rows.empty ()) return {};

            auto &entry = rows.front ();

            Bitcoin::header header {data::slice<data::byte, 80> {std::get<0> (entry).data ()}};

            Gigamonkey::chain_loader {}.set_hash (header, std::get<1> (entry));

            return std::make_shared<const data::entry<N, Bitcoin::header>> (height, header);
        }

        block_header latest () final override {
            auto rows = storage.select (
                columns (&Block::header, &Block::hash, &Block::height),
                order_by (&Block::height).desc (), limit (1));

            if (rows.empty ()) throw data::exception {} << "invalid database; there must be at least one block header";

            auto &entry = rows.front ();

            Bitcoin::header header {data::slice<data::byte, 80> {std::get<0> (entry).data ()}};

            Gigamonkey::chain_loader {}.set_hash (header, std::get<1> (entry));

            return std::make_shared<const data::entry<N, Bitcoin::header>> (std::get<2> (entry), header);
        }

        // get by hash or merkle root (need both)
        block_header header (const digest256 &hash_or_root) final override {
            auto rows = storage.select (
                columns (&Block::header, &Block::hash, &Block::height),
                    where (is_equal (&Block::hash, hash_or_root) or is_equal (&Block::root, hash_or_root)), limit (1));

            if (rows.empty ()) return {};

            auto &entry = rows.front ();

            Bitcoin::header header {data::slice<data::byte, 80> {std::get<0> (entry).data ()}};

            Gigamonkey::chain_loader {}.set_hash (header, std::get<1> (entry));

            return std::make_shared<const data::entry<N, Bitcoin::header>> (std::get<2> (entry), header);
        }

        block_header insert (const data::N &height, const Bitcoin::header &h) final override {
            try {
                storage.insert (Block {h.hash (), uint64_t (height), h.MerkleRoot, h.write ()});
            } catch (const std::system_error &e) {
                // means the row already exits.
            }

            return std::make_shared<const data::entry<N, Bitcoin::header>> (height, h);

        }

        void move_txid_to_pending (const Bitcoin::TXID &txid) {

            storage.update_all (
                sqlite_orm::set (
                    assign (&Transaction::height, optional<uint64_t> {}),
                    assign (&Transaction::status, Transaction::pending)
                ), where (is_equal (&Transaction::hash, txid)));
        }

        void remove_header (const data::N &n) final override {

            auto rows = storage.select (
                columns (&Block::height, &Block::merkle_tree),
                order_by (&Block::height).desc (), limit (1));

            if (rows.empty ()) throw data::exception {} << "invalid database; there must be at least one block header";

            auto entry = rows.front ();

            if (n != std::get<0> (entry)) return;

            for (const auto &txid : Merkle::BUMP {std::get<1> (entry)}.paths ().keys ())
                move_txid_to_pending (txid);

        }

        void remove_header (const digest256 &d) final override {

            auto rows = storage.select (
                columns (&Block::hash, &Block::merkle_tree),
                order_by (&Block::height).desc (), limit (1));

            if (rows.empty ()) throw data::exception {} << "invalid database; there must be at least one block header";

            auto entry = rows.front ();

            if (d != std::get<0> (entry)) return;

            for (const auto &txid : Merkle::BUMP {std::get<1> (entry)}.paths ().keys ())
                move_txid_to_pending (txid);
        }

        std::tuple<Bitcoin::header, Merkle::dual> get_confirmation (uint64_t height) {
            auto rows = storage.select (
                columns (&Block::header, &Block::hash, &Block::merkle_tree),
                    where (is_equal (&Block::height, height)), limit (1));

            if (rows.empty ()) throw data::exception {} << "corrupt database";

            auto &entry = rows.front ();

            Bitcoin::header header {data::slice<data::byte, 80> {std::get<0> (entry).data ()}};

            Gigamonkey::chain_loader {}.set_hash (header, std::get<1> (entry));

            return {header, Merkle::dual {Merkle::BUMP {std::get<2> (entry)}.paths (), header.MerkleRoot}};
        }

        // do we have a tx or merkle proof for a given tx?
        tx transaction (const Bitcoin::TXID &txid) final override {
            // Step 1: Get the transaction by hash
            auto tx_opt = storage.get_optional<Transaction> (txid);
            if (!tx_opt) return {};

            if (!bool (tx_opt->height)) return tx {std::make_shared<Bitcoin::transaction> (tx_opt->tx)};

            auto block_opt = storage.get_optional<Block> (*tx_opt->height);
            if (!block_opt) throw data::exception {} << "corrupt database";

            std::tuple<Bitcoin::header, Merkle::dual> conf = get_confirmation (*tx_opt->height);

            return tx {std::make_shared<Bitcoin::transaction> (tx_opt->tx),
                SPV::confirmation {
                    Merkle::path (std::get<Merkle::dual> (conf)[txid].Branch),
                    N (*tx_opt->height),
                    std::get<Bitcoin::header> (conf)}};
        }

        optional<Merkle::BUMP> get_BUMP (const digest256 &hash_or_root) {
            auto rows = storage.select (
                columns (&Block::header, &Block::merkle_tree),
                    where (is_equal (&Block::hash, hash_or_root) or is_equal (&Block::root, hash_or_root)), limit (1));

            if (rows.empty ()) return {};

            auto &entry = rows.front ();

            return Merkle::BUMP {std::get<1> (entry)};
        }

        bool insert (const Merkle::dual &dd) final override {
            if (!dd.valid ()) return false;

            optional<Merkle::BUMP> bump = get_BUMP (dd.Root);

            if (!bool (bump)) return false;

            Merkle::dual new_tree = Merkle::dual {bump->paths (), dd.Root} + dd;

            storage.update_all (
                sqlite_orm::set (assign (&Block::merkle_tree, bytes (Merkle::BUMP {bump->BlockHeight, new_tree.Paths}))),
                where (is_equal (&Block::height, bump->BlockHeight)));

            // move transaction with these txids to mined.
            for (const auto &txid : dd.Paths.keys ())
                storage.update_all (
                    sqlite_orm::set (
                        assign (&Transaction::height, optional<uint64_t> {bump->BlockHeight}),
                        assign (&Transaction::status, Transaction::mined)
                    ), where (is_equal (&Transaction::hash, txid)));

            return true;
        }

        digest256 add_script (const data::bytes &script) final override {
            auto hash = Gigamonkey::SHA2_256 (script);
            try {
                storage.insert (
                    sqlite_orm::into<Script> (),
                    columns (&Script::hash, &Script::script),
                    values (hash, script));
            } catch (const std::system_error &e) {
                // means the row already exits.
            }

            return hash;
        }

        // associate a script with a given hash with an output.
        void add_output (const digest256 &script_hash, const Bitcoin::outpoint &o) final override {
            try {
                storage.insert (
                    sqlite_orm::into<Output> (),
                    columns (&Output::outpoint, &Output::script_hash),
                    values (o, script_hash));
            } catch (const std::system_error &e) {
                // means the row already exits.
            }
        }

        void insert (const Transaction &tx) {
            try {
                storage.insert (
                    sqlite_orm::into<Transaction> (),
                    columns (&Transaction::hash, &Transaction::tx, &Transaction::height, &Transaction::status),
                    values (tx.hash, tx.tx, tx.height, tx.status));
            } catch (const std::system_error &e) {

            }
        }

        bool insert (const Bitcoin::transaction &tx, const Merkle::path &path) final override {
            const auto &hash = tx.id ();

            Merkle::branch branch {hash, path};
            digest256 root = branch.root ();

            optional<Merkle::BUMP> bump = get_BUMP (root);

            if (!bool (bump)) return false;

            Merkle::dual new_tree = Merkle::dual {bump->paths (), root} + Merkle::proof {branch, branch.root ()};

            storage.update_all (
                sqlite_orm::set (assign (&Block::merkle_tree, bytes (Merkle::BUMP {bump->BlockHeight, new_tree.Paths}))),
                where (is_equal (&Block::height, bump->BlockHeight)));

            insert (Transaction {tx.id (), tx.write (), bump->BlockHeight, Transaction::mined});

            return true;
        }

        void insert (const Bitcoin::transaction &tx) final override {
            insert (Transaction {tx.id (), tx.write (), {}, Transaction::pending});
        }

        // get txids for transactions without Merkle proofs.
        data::set<Bitcoin::TXID> unconfirmed () final override {
            auto rows = storage.select (
                columns (&Transaction::hash),
                    where (is_equal (&Transaction::status, Transaction::pending)));

            data::set<Bitcoin::TXID> unconf {};
            for (const auto &row : rows) unconf = unconf.insert (std::get<0> (row));
            return unconf;
        }

        void set_redeem (const Bitcoin::outpoint &o, const inpoint &i) final override {
            try {
                storage.insert (
                    sqlite_orm::into<Redemption> (),
                    columns (&Redemption::outpoint, &Redemption::inpoint),
                    values (o, i));
            } catch (const std::system_error &e) {
                // means the row already exits.
            }
        }

        event redeeming (const Bitcoin::outpoint &o) final override {
            auto redeem_rows = storage.select (
                columns (&Redemption::inpoint),
                    where (is_equal (&Redemption::outpoint, o)), limit (1));

            if (redeem_rows.empty ()) return {};

            auto &inpoint = std::get<0> (redeem_rows.front ());

            return event {this->operator [] (inpoint.Digest), inpoint.Index, direction::in};
        }

        void add_address (const Bitcoin::address &addr, const digest256 &script_hash) final override {
            try {
                storage.insert (
                    sqlite_orm::into<Address> (),
                    columns (&Address::script_hash, &Address::address),
                    values (script_hash, addr));
            } catch (const std::system_error &e) {
                // means the row already exits.
            }
        }

        events by_script_hash (const digest256 &hash) final override {
            auto rows = storage.select (
                columns (&Output::outpoint),
                where (is_equal (&Output::script_hash, hash)));

            events results;

            for (const auto &[outpoint] : rows) {
                auto v = this->operator [] (outpoint.Digest);
                // this shouldn't happen.
                if (!bool (v)) throw data::exception {} << "missing transaction " << outpoint.Digest;
                results = results.insert (event {v, outpoint.Index, direction::out});
                // now look for the redeeming event.
                auto r = redeeming (outpoint);
                if (bool (r)) results = results.insert (r);
            }

            return results;
        }

        events by_address (const Bitcoin::address &addr) final override {
            auto rows = storage.select (
                columns (&Address::script_hash),
                where (is_equal (&Address::address, addr)));

            events results;

            for (const auto &[script_hash] : rows)
                for (const event &e : data::reverse (by_script_hash (script_hash)))
                    results = results.insert (e);

            return results;
        }

        // can only remove txs in pending.
        void remove (const Bitcoin::TXID &hash) final override {
            auto rows = storage.select (
                columns (&Transaction::tx, &Transaction::status),
                where (is_equal (&Transaction::hash, hash)), limit (1));
            if (rows.empty ()) return;

            const auto &entry = rows.front ();

            if (std::get<1> (entry) != Transaction::pending) return;

            Bitcoin::transaction tx {std::get<0> (entry)};

            storage.remove_all<Transaction> (where (is_equal (&Transaction::hash, hash)));

            // remove from redemptions
            for (const auto &in : tx.Inputs)
                storage.remove_all<Redemption> (where (is_equal (&Redemption::outpoint, in.Reference)));

            // remove from outputs
            for (uint32 output_index = 0; output_index < tx.Outputs.size (); output_index++)
                storage.remove_all<Output> (where (is_equal (&Output::outpoint, Bitcoin::outpoint {hash, output_index})));

        }

        /*
            historical prices database
        */

        constexpr static const uint32 half_day_seconds = 60 * 60 * 24 / 2 + 1;

        maybe<double> get_price (monetary_unit, const Bitcoin::timestamp &t) final override {
            auto rows = storage.select (
                columns (&Price::unit, &Price::timestamp, &Price::price),
                order_by (sqlite_orm::abs (c (&Price::timestamp) - int64_t (t.Value))),
                limit (1));

            if (rows.empty ()) return {};

            const auto &entry = rows.front ();

            if (data::abs (std::get<1> (entry) - int64_t (t.Value)) > half_day_seconds) return {};

            else return {std::get<2> (entry)};
        }

        void set_price (monetary_unit u, const Bitcoin::timestamp &t, double price) final override {
            std::stringstream mu;
            mu << u;
            storage.insert (Price {-1, mu.str (), t.Value, price});
        }

        /*
            keys and hashes
        */

        bool set_invert_hash (data::slice<const data::byte> digest, hash_function f, data::slice<const data::byte> data) final override {

            if (!supported (f)) return false;
            
            try {
                storage.insert (
                    sqlite_orm::into<Digest> (), 
                    columns (&Digest::digest, &Digest::function, &Digest::data), 
                    values (data::bytes {digest}, data::byte (f), data::bytes {data}));
            } catch (const std::system_error &e) {
                return false;
            }

            return true;
        }

        data::maybe<std::tuple<Cosmos::hash_function, data::bytes>> get_invert_hash (data::slice<const data::byte> dig) final override {
            
            data::bytes doob {dig};
            
            auto rows = storage.select (
                columns (&Digest::digest, &Digest::function, &Digest::data),
                where (is_equal (&Digest::digest, doob)), limit (1));

            if (rows.empty ()) return {};
            
            const auto &entry = rows.front ();

            return data::maybe<std::tuple<Cosmos::hash_function, data::bytes>> {std::tuple<Cosmos::hash_function, data::bytes> {
                Cosmos::hash_function (std::get<1> (entry)), 
                data::bytes {std::get<2> (entry)}
            }};
        }

        bool set_key (const std::string &key_name, const key_expression &k) final override {

            try {
                storage.insert (
                    sqlite_orm::into<Secret> (),
                    columns (&Secret::name, &Secret::key),
                    values (key_name, std::string (k)));
            } catch (const std::system_error &e) {
                return false;
            }

            return true;
        }

        key_expression get_key (const std::string &key_name) final override {
            auto rows = storage.select (
                columns (&Secret::name, &Secret::key),
                where (is_equal (&Secret::name, key_name)), limit (1));

            if (rows.empty ()) return {};
            
            const auto &entry = rows.front ();

            return key_expression {std::get<1> (entry)};
        }

        bool set_to_private (const std::string &key_name, const key_expression &k) final override {
            try {
                storage.insert (
                    sqlite_orm::into<Pubkey> (),
                    columns (&Pubkey::name, &Pubkey::key),
                    values (key_name, std::string (k)));
            } catch (const std::system_error &e) {
                return false;
            }

            return true;
        }
        
        key_expression get_to_private (const std::string &key_name) final override {
            auto rows = storage.select (
                columns (&Pubkey::name, &Pubkey::key),
                where (is_equal (&Pubkey::name, key_name)), limit (1));

            if (rows.empty ()) return {};
            
            const auto &entry = rows.front ();

            return key_expression {std::get<1> (entry)};
        }

        /*
            wallet database
        */

        bool make_wallet (const std::string &name) final override {
            try {
                storage.insert (Wallet {-1, name});
            } catch (const std::system_error &e) {
                return false;
            }

            return true;
        }

        list<std::string> list_wallet_names () final override {
            list<std::string> names;
            for (const auto &name : storage.select (&Wallet::name)) names <<= name;
            return names;
        }

        bool set_wallet_derivation (const std::string &wallet_name, const std::string &deriv_name, const derivation &) {
            throw data::method::unimplemented {"SQLite::set_derivation"};
        }

        list<derivation> get_wallet_derivations (const std::string &wallet_name) final override {
            throw data::method::unimplemented {"SQLite::get_wallet_derivations"};
        };

        maybe<derivation> get_wallet_derivation (const std::string &wallet_name, const std::string &deriv_name) final override {
            throw data::method::unimplemented {"SQLite::get_wallet_derivation"};
        };

        Cosmos::account get_wallet_account (const std::string &wallet_name) final override {
            throw data::method::unimplemented {"SQLite::get_wallet_account"};
        };

        void set_wallet_unused (const std::string &wallet_name, const std::string &key_name) final override {
            throw data::method::unimplemented {"SQLite::set_wallet_unused"};
        };

        void set_wallet_used (const std::string &wallet_name, const std::string &key_name) final override {
            throw data::method::unimplemented {"SQLite::set_wallet_used"};
        };

        data::map<Diophant::symbol, key_expression> get_wallet_symbols (const std::string &wallet_name) final override {
            throw data::method::unimplemented {"SQLite::get_wallet_symbols"};
        };

    };

    ptr<database> load (const data::maybe<filepath> &fzf) {
        std::string path;
        if (!bool (fzf)) path = ":memory:";
        else path = *fzf;
        return std::static_pointer_cast<database> (std::make_shared<db> (path));
    }

}
