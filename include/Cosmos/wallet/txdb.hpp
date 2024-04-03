#ifndef COSMOS_WALLET_TXDB
#define COSMOS_WALLET_TXDB

#include <Cosmos/wallet/write.hpp>
#include <Cosmos/network.hpp>

namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace Merkle = Gigamonkey::Merkle;

    // a transaction that has been mined into a block by the network.
    struct processed {
        bytes Transaction;

        struct merkle_proof {
            Merkle::path Path;
            digest256 BlockHash;
        };

        merkle_proof *Proof;

        processed (): Transaction {}, Proof {nullptr} {}
        processed (const bytes &tx, const Merkle::path &p, const digest256 h) : Transaction {tx}, Proof {new merkle_proof {p, h}} {}

        explicit operator JSON () const;
        processed (const JSON &);

        struct unmined : std::exception {
            static constexpr char Error[] = "Unmined tx";
            const char *what () const noexcept override {
                return Error;
            }
        };

        bool operator == (const processed &p) const {
            if (Proof == nullptr) throw unmined {};
            return Transaction == p.Transaction && Proof->Path == Proof->Path && Proof->BlockHash == Proof->BlockHash;
        }

        bool verify (const Bitcoin::txid &id, const byte_array<80> &h) const;

        Bitcoin::txid id () const {
            return Bitcoin::transaction::id (Transaction);
        }

        bool valid () const {
            if (Proof == nullptr) throw unmined {};
            return Transaction.size () != 0 && data::valid (Proof->Path) && Proof->BlockHash.valid ();
        }

    };

    // a transaction with a complete Merkle proof.
    struct vertex {
        const processed &Processed;
        const byte_array<80> &Header;

        std::strong_ordering operator <=> (const vertex &tx) const {
            auto compare_time = Bitcoin::header::timestamp (Header) <=> Bitcoin::header::timestamp (tx.Header);
            if (compare_time == std::strong_ordering::equal) return Processed.Proof->Path.Index <=> tx.Processed.Proof->Path.Index;
            return compare_time;
        }

        bool operator == (const vertex &e) const {
            return Processed == e.Processed && Header == e.Header;
        }

        bool valid () const {
            return Processed.verify (Processed.id (), Header);
        }

        Bitcoin::timestamp when () const {
            return Bitcoin::header::timestamp (Header);
        }
    };

    enum class direction {
        in,
        out
    };

    // a tx with a complete merkle proof and an indication of a specific input or output.
    struct ray {

        bytes Put;
        Bitcoin::timestamp When;
        uint64 Index;
        direction Direction;
        Bitcoin::outpoint Point;
        Bitcoin::satoshi Value;

        ray (Bitcoin::timestamp w, uint32 i, const Bitcoin::outpoint &op, const Bitcoin::output &o) :
            Put {bytes (o)}, When {w}, Index {i}, Direction {direction::out}, Point {op}, Value {o.Value} {}

        ray (Bitcoin::timestamp w, uint32 i, const inpoint &ip, const Bitcoin::input &in, const Bitcoin::satoshi &v) :
            Put {bytes (in)}, When {w}, Index {i}, Direction {direction::in}, Point {ip}, Value {v} {}

        ray (const vertex &t, const Bitcoin::outpoint &op):
            Put {Bitcoin::transaction::output (t.Processed.Transaction, op.Index)}, When {t.when ()}, Index {t.Processed.Proof->Path.Index},
            Direction {direction::out}, Point {op}, Value {Bitcoin::output::value (Put)} {}

        ray (const vertex &t, const inpoint &ip, const Bitcoin::satoshi &v):
            Put {Bitcoin::transaction::input (t.Processed.Transaction, ip.Index)}, When {t.when ()}, Index {t.Processed.Proof->Path.Index},
            Direction {direction::in}, Point {ip}, Value {v} {}

        std::strong_ordering operator <=> (const ray &e) const {
            auto compare_time = When <=> e.When;
            if (compare_time != std::strong_ordering::equal) return compare_time;
            auto compare_index = Index <=> e.Index;
            if (compare_index != std::strong_ordering::equal) return compare_index;
            if (Direction != e.Direction) return direction::in <=> direction::out;
            return Point.Index <=> e.Point.Index;
        }

        bool operator == (const ray &e) const {
            return Put == e.Put && When == e.When &&
                Direction == e.Direction && Point.Index == e.Point.Index;
        }

    };

    // a database of transactions.
    struct txdb {
        virtual ptr<vertex> operator [] (const Bitcoin::txid &) = 0;

        Bitcoin::output output (const Bitcoin::outpoint &p) {
            auto tx = operator [] (p.Digest);
            if (tx == nullptr) return {};
            return Bitcoin::output {Bitcoin::transaction::output (tx->Processed.Transaction, p.Index)};
        }

        Bitcoin::satoshi value (const Bitcoin::outpoint &p) {
            return output (p).Value;
        }

        virtual ordered_list<ray> by_address (const Bitcoin::address &) = 0;
        virtual ordered_list<ray> by_script_hash (const digest256 &) = 0;
        virtual ptr<ray> redeeming (const Bitcoin::outpoint &) = 0;

        virtual ~txdb () {}

    };

    struct local_txdb final : txdb {
        std::map<Bitcoin::txid, processed> Transactions;
        std::map<Bitcoin::txid, byte_array<80>> Headers;

        std::map<Bitcoin::address, list<Bitcoin::outpoint>> AddressIndex;
        std::map<digest256, list<Bitcoin::outpoint>> ScriptIndex;
        std::map<Bitcoin::outpoint, inpoint> RedeemIndex;

        bool import_transaction (const processed &p, const byte_array<80> &h);

        local_txdb (): txdb {}, Transactions {}, Headers {} {}
        explicit local_txdb (const JSON &);
        explicit operator JSON () const;
        ptr<vertex> operator [] (const Bitcoin::txid &) override;

        ordered_list<ray> by_address (const Bitcoin::address &) override;
        ordered_list<ray> by_script_hash (const digest256 &) override;
        ptr<ray> redeeming (const Bitcoin::outpoint &) override;
    };

    struct cached_remote_txdb final : txdb {
        network &Net;
        local_txdb Local;

        cached_remote_txdb (network &n, local_txdb &x): Net {n}, Local {x} {}

        ptr<vertex> operator [] (const Bitcoin::txid &) override;

        ordered_list<ray> by_address (const Bitcoin::address &) override;
        ordered_list<ray> by_script_hash (const digest256 &) override;
        ptr<ray> redeeming (const Bitcoin::outpoint &) override;

        void import_transaction (const Bitcoin::txid &);
    };

    struct price_data {
        network &Net;
        std::map<Bitcoin::timestamp, double> Price;
        double operator [] (const Bitcoin::timestamp &t);
        price_data (network &n) : Net {n} {}
        price_data (network &n, const JSON &);
        explicit operator JSON () const;
    };

    local_txdb inline read_local_txdb_from_file (const std::string &filename) {
        return local_txdb (read_from_file (filename));
    }

}

#endif
