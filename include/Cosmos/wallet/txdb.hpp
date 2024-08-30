#ifndef COSMOS_WALLET_TXDB
#define COSMOS_WALLET_TXDB

#include <Cosmos/wallet/write.hpp>
#include <Cosmos/network.hpp>

namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace Merkle = Gigamonkey::Merkle;

    // a transaction with a complete Merkle proof.
    struct vertex : SPV::database::confirmed {
        using SPV::database::confirmed::confirmed;
        vertex (SPV::database::confirmed &&c) : SPV::database::confirmed {std::move (c)} {}

        std::strong_ordering operator <=> (const vertex &tx) const {
            if (!valid ()) throw exception {} << "unconfirmed or invalid tx.";
            return *this->Confirmation <=> *tx.Confirmation;
        }

        bool operator == (const vertex &tx) const {
            if (!valid ()) throw exception {} << "unconfirmed or invalid tx.";
            return *this->Transaction == *tx.Transaction && *this->Confirmation == *tx.Confirmation;
        }

        bool valid () const {
            return this->Transaction != nullptr && bool (this->Confirmation);
        }

        explicit operator bool () {
            return valid ();
        }

        Bitcoin::timestamp when () const {
            return this->Confirmation->Header.Timestamp;
        }
    };

    enum class direction {
        in,
        out
    };

    // a tx with a complete merkle proof and an indication of a specific input or output.
    struct ray {
        // in our out.
        direction Direction;
        // output or input depending on which it is.
        bytes Put;
        Bitcoin::timestamp When;
        // the index of the transaction in the block.
        uint64 Index;
        Bitcoin::outpoint Point;
        Bitcoin::satoshi Value;

        ray (Bitcoin::timestamp w, uint32 i, const Bitcoin::outpoint &op, const Bitcoin::output &o) :
            Direction {direction::out}, Put {bytes (o)}, When {w}, Index {i}, Point {op}, Value {o.Value} {}

        ray (Bitcoin::timestamp w, uint32 i, const inpoint &ip, const Bitcoin::input &in, const Bitcoin::satoshi &v) :
            Direction {direction::in}, Put {bytes (in)}, When {w}, Index {i}, Point {ip}, Value {v} {}

        ray (const vertex &t, const Bitcoin::outpoint &op):
            Direction {direction::out},
            Put {bytes (t.Transaction->Outputs[op.Index])},
            When {t.when ()},
            Index {t.Confirmation->Path.Index},
            Point {op}, Value {Bitcoin::output::value (Put)} {}

        ray (const vertex &t, const inpoint &ip, const Bitcoin::satoshi &v):
            Direction {direction::in},
            Put {bytes (t.Transaction->Inputs[ip.Index])},
            When {t.when ()},
            Index {t.Confirmation->Path.Index},
            Point {ip}, Value {v} {}

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

    std::ostream inline &operator << (std::ostream &o, const ray &r) {
        return o << "\n\t" << r.Value << " " << (r.Direction == direction::in ? "received in " : "spent from ") << r.Point << std::endl;
    }

    // a database of transactions.
    struct txdb {

        virtual vertex operator [] (const Bitcoin::TXID &id) = 0;
        virtual ordered_list<ray> by_address (const Bitcoin::address &) = 0;
        virtual ordered_list<ray> by_script_hash (const digest256 &) = 0;
        virtual ptr<ray> redeeming (const Bitcoin::outpoint &) = 0;

        Bitcoin::output output (const Bitcoin::outpoint &p) {
            vertex tx = (*this)[p.Digest];
            if (!tx.valid ()) return {};
            return tx.Transaction->Outputs[p.Index];
        }

        Bitcoin::satoshi value (const Bitcoin::outpoint &p) {
            return output (p).Value;
        }

        virtual ~txdb () {}

    };

    struct local_txdb final : SPV::database::memory, txdb {

        std::map<Bitcoin::address, list<Bitcoin::outpoint>> AddressIndex;
        std::map<digest256, list<Bitcoin::outpoint>> ScriptIndex;
        std::map<Bitcoin::outpoint, inpoint> RedeemIndex;

        bool import_transaction (const Bitcoin::transaction &, const Merkle::path &, const Bitcoin::header &h);

        vertex operator [] (const Bitcoin::TXID &id) final override {
            return vertex {this->tx (id)};
        }

        ordered_list<ray> by_address (const Bitcoin::address &) final override;
        ordered_list<ray> by_script_hash (const digest256 &) final override;
        ptr<ray> redeeming (const Bitcoin::outpoint &) final override;

        local_txdb (): SPV::database::memory {}, txdb {}, AddressIndex {}, ScriptIndex {}, RedeemIndex {} {}
        explicit local_txdb (const JSON &);
        explicit operator JSON () const;
    };

    struct cached_remote_txdb final : txdb {
        network &Net;
        local_txdb Local;

        cached_remote_txdb (network &n, local_txdb &x): txdb {}, Net {n}, Local {x} {}

        vertex operator [] (const Bitcoin::TXID &id) final override;
        ordered_list<ray> by_address (const Bitcoin::address &) final override;
        ordered_list<ray> by_script_hash (const digest256 &) final override;
        ptr<ray> redeeming (const Bitcoin::outpoint &) final override;

        void import_transaction (const Bitcoin::TXID &);
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
