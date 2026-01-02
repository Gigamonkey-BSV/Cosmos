#ifndef COSMOS_WALLET_SPLIT
#define COSMOS_WALLET_SPLIT

#include <gigamonkey/timechain.hpp>
#include <data/random.hpp>
#include <Cosmos/wallet/spend.hpp>
#include <Cosmos/options.hpp>

namespace Cosmos {

    // This is for splitting a wallet into tons of tiny outputs.
    struct split {

        // split options
        Bitcoin::satoshi MinSatsPerOutput {spend_options::DefaultMinSatsPerOutput};
        Bitcoin::satoshi MaxSatsPerOutput {spend_options::DefaultMaxSatsPerOutput};
        double MeanSatsPerOutput {spend_options::DefaultMeanSatsPerOutput};

        split (
            Bitcoin::satoshi min_sats_per_output = spend_options::DefaultMinSatsPerOutput,
            Bitcoin::satoshi max_sats_per_output = spend_options::DefaultMaxSatsPerOutput,
            double mean_sats_per_output = spend_options::DefaultMeanSatsPerOutput);

        // the user provides the outputs to split and this function does the rest.
        spend::spent operator () (
            data::random::entropy &, // we don't really need cryptographically secure random here.
            account,
            selected z,
            double fee_rate) const;

        struct result {
            list<std::pair<extended_transaction, account_diff>> Transactions;
            uint32 Last;
        };
/*
        // this function handles everything other than updating the addresses.
        result operator () (data::entropy &, keychain, pubkeys,
            address_sequence, list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const;*/

        struct result_outputs {
            // list of new outputs; these are not shuffled so be sure to do that if you use these in a tx.
            list<redeemable> Outputs;
            uint32 Last;
        };

        // construct only the outputs.
        result_outputs construct_outputs (data::random::entropy &r, const key_source &key,
            Bitcoin::satoshi split_value, double fee_rate) const;

        // we use this to make a
        math::log_triangular_distribution LogTriangular;
    };

    // use the same function that we use to split coins for generating change outputs.
    struct split_change_parameters : split {
        // the minimum value of a change output. Below this value, no output will be created.
        Bitcoin::satoshi MinimumCreateValue {spend_options::DefaultMinChangeSats};

        // construct a set of change outputs.
        change operator () (const key_source &x, Bitcoin::satoshi sats, satoshis_per_byte fees, data::random::entropy &rand) const;
        using split::operator ();

        split_change_parameters (
            Bitcoin::satoshi min_change_value = spend_options::DefaultMinChangeSats,
            Bitcoin::satoshi min_sats_per_output = spend_options::DefaultMinSatsPerOutput,
            Bitcoin::satoshi max_sats_per_output = spend_options::DefaultMaxSatsPerOutput,
            double mean_sats_per_output = spend_options::DefaultMeanSatsPerOutput) :
            split {min_sats_per_output, max_sats_per_output, mean_sats_per_output},
            MinimumCreateValue {min_change_value} {}

        split_change_parameters (const spend_options &o) :
            split_change_parameters {o.MinChangeSats, o.MinSatsPerOutput, o.MaxSatsPerOutput, o.MeanSatsPerOutput} {}
    };
/*
    spend::spent inline split::operator () (data::entropy &rand, account acc,
        list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const {
        result rr = (*this) (rand, k, w.Pubkeys, w.Addresses.change (), selected, fee_rate);
        return spend::spent {rr.Transactions, w.Addresses.update (w.Addresses.Change, rr.Last)};
    }*/

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
