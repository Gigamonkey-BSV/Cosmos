#ifndef COSMOS_WALLET_SPLIT
#define COSMOS_WALLET_SPLIT

#include <gigamonkey/timechain.hpp>
#include <data/crypto/random.hpp>
#include <Cosmos/wallet/spend.hpp>
#include <data/math/probability/triangular_distribution.hpp>
#include <Cosmos/options.hpp>

namespace Cosmos {

    // Given a general goal of splitting a wallet into many tiny outputs,
    // this type provides several possible steps in such a process.
    struct split {
        Bitcoin::satoshi MinSatsPerOutput {spend_options::DefaultMinSatsPerOutput};
        Bitcoin::satoshi MaxSatsPerOutput {spend_options::DefaultMaxSatsPerOutput};
        double MeanSatsPerOutput {spend_options::DefaultMeanSatsPerOutput};

        split (
            Bitcoin::satoshi min_sats_per_output = spend_options::DefaultMinSatsPerOutput,
            Bitcoin::satoshi max_sats_per_output = spend_options::DefaultMaxSatsPerOutput,
            double mean_sats_per_output = spend_options::DefaultMeanSatsPerOutput);

        // the user provides the outputs to split and this function does the rest.
        spend::spent operator () (
            data::crypto::entropy &, account,
            list<entry<Bitcoin::outpoint, redeemable>> selected,
            double fee_rate) const;
/*
        struct result {
            list<std::pair<extended_transaction, account_diff>> Transactions;
            uint32 Last;
        };

        // this function handles everything other than updating the addresses.
        result operator () (data::crypto::random &, keychain, pubkeys,
            address_sequence, list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const;*/

        struct result_outputs {
            // list of new outputs; these are not shuffled so be sure to do that if you use these in a tx.
            list<redeemable> Outputs;
            uint32 Last;
        };

        // construct only the outputs.
        result_outputs operator () (data::crypto::entropy &r, address_source key,
            Bitcoin::satoshi value, double fee_rate) const;

        struct log_triangular_distribution {
            math::triangular_distribution<double> Triangular;

            log_triangular_distribution (double min, double max, double mean);

            template <std::uniform_random_bit_generator engine>
            double operator () (engine &e) const;

        };

        log_triangular_distribution LogTriangular;

        static double inline ln (double x) {
            return std::log (x);
        }

        static double inline exp (double x) {
            return std::exp (x);
        }
    };

    // use the same function that we use to split coins for generating change outputs.
    struct split_change_parameters : split {
        // the minimum value of a change output. Below this value, no output will be created.
        Bitcoin::satoshi MinimumCreateValue {spend_options::DefaultMinChangeValue};

        // construct a set of change outputs.
        change operator () (address_source x, Bitcoin::satoshi sats, satoshis_per_byte fees, data::crypto::entropy &rand) const;
        using split::operator ();

        split_change_parameters (
            Bitcoin::satoshi min_change_value = spend_options::DefaultMinChangeValue,
            Bitcoin::satoshi min_sats_per_output = spend_options::DefaultMinSatsPerOutput,
            Bitcoin::satoshi max_sats_per_output = spend_options::DefaultMaxSatsPerOutput,
            double mean_sats_per_output = spend_options::DefaultMeanSatsPerOutput) :
            split {min_sats_per_output, max_sats_per_output, mean_sats_per_output},
            MinimumCreateValue {min_change_value} {}

        split_change_parameters (const spend_options &o) :
            split_change_parameters {o.MinChangeValue, o.MinSatsPerOutput, o.MaxSatsPerOutput, o.MeanSatsPerOutput} {}
    };
/*
    spend::spent inline split::operator () (data::crypto::entropy &rand, account acc,
        list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const {
        result rr = (*this) (rand, k, w.Pubkeys, w.Addresses.change (), selected, fee_rate);
        return spend::spent {rr.Transactions, w.Addresses.update (w.Addresses.Change, rr.Last)};
    }*/

    template <std::uniform_random_bit_generator engine>
    double inline split::log_triangular_distribution::operator () (engine &e) const {
        return exp (Triangular (e));
    }

    inline split::split (
        Bitcoin::satoshi min_sats_per_output,
        Bitcoin::satoshi max_sats_per_output,
        double mean_sats_per_output) :
        MinSatsPerOutput {min_sats_per_output},
        MaxSatsPerOutput {max_sats_per_output},
        MeanSatsPerOutput {mean_sats_per_output},
        LogTriangular {
            static_cast<double> (min_sats_per_output),
            static_cast<double> (max_sats_per_output),
            mean_sats_per_output} {}
}

#endif
