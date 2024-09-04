#include <Cosmos/boost/miner_options.hpp>
#include <argh.h>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/schema/hd.hpp>

namespace BoostPOW {
/*
    int run_redeem (const argh::parser &command_line,
        int (*redeem) (const Bitcoin::outpoint &, const bytes &script, int64_t value, const redeeming_options &));

    int run_mine (const argh::parser &command_line, int (*mine) (const mining_options &));

    int run (const argh::parser &command_line,
        int (*help) (),
        int (*version) (),
        int (*spend) (const script_options &),
        int (*redeem) (const Bitcoin::outpoint &, const bytes &script, int64 value, const redeeming_options &),
        int (*mine) (const mining_options &)) {

        try {
            if (command_line["version"]) return version ();

            if (command_line["help"]) return help ();

            if (!command_line (1)) throw data::exception {"Must provide a command (help, version, spend, redeem, mine)"};

            string method = command_line (1).str ();

            if (method == "help") return help ();

            if (method == "version") return version ();

            if (method == "spend") return spend (script_options::read (command_line));

            if (method == "redeem") return run_redeem (command_line, redeem);

            if (method == "mine") return run_mine (command_line, mine);

            throw data::exception {} << "Invalid method " << method << " called. Must be help, version, spend, redeem, or mine";

        } catch (const std::string x) {
            std::cout << "Error: " << x << std::endl;
            return 1;
        } catch (const data::exception &x) {
            std::cout << "Error: " << x.what() << std::endl;
            return 1;
        } catch (const std::exception &x) {
            std::cout << "Unexpected error: " << x.what() << std::endl;
            return 1;
        } catch (...) {
            std::cout << "Unknown error " << std::endl;
            return 1;
        }

        return 1;
    }*/

    script_options script_options::read (const argh::parser &command_line, int start_pos) {

        script_options options;

        string content_hex;

        if (auto positional = command_line (start_pos); positional) positional >> content_hex;
        else if (auto option = command_line ("content"); option) option >> content_hex;
        else throw data::exception {"option content not provided "};

        options.Content = digest256 {content_hex};
        if (!options.Content.valid ()) throw data::exception {} << "invalid content string: " << content_hex;

        if (auto positional = command_line (start_pos + 1); positional) positional >> options.Difficulty;
        else if (auto option = command_line ("difficulty"); option) option >> options.Difficulty;
        else throw data::exception {"option difficulty not provided "};

        if (options.Difficulty <= 0) throw data::exception {} << "difficulty must be >= 0; value provided was " << options.Difficulty;

        if (auto positional = command_line (start_pos + 2); positional) options.Topic = positional.str ();
        else if (auto option = command_line ("topic"); option) options.Topic = option.str ();

        if (auto positional = command_line (start_pos + 3); positional) options.Data = positional.str ();
        else if (auto option = command_line ("data"); option) options.Data = option.str ();

        string address;

        if (auto positional = command_line (start_pos + 4); positional) positional >> address;
        else if (auto option = command_line ("address"); option) option >> address;

        if (address != "") {
            Bitcoin::address miner_address {address};
            if (!miner_address.valid ()) throw data::exception {} << "invalid address provided: " << address;
            options.MinerPubkeyHash = miner_address.digest ();
        }

        if (auto option = command_line ("version"); option) option >> options.Version;

        if (options.Version < 1 || options.Version > 2) throw data::exception {} << "invalid script version " << options.Version;

        if (auto option = command_line ("user_nonce"); option) {
            uint32 user_nonce;
            option >> user_nonce;
            options.UserNonce = user_nonce;
        }

        if (auto option = command_line ("category"); option) {
            uint32 category;
            option >> category;
            options.Category = category;
        }

        return options;
    }

    script_options::operator Boost::output_script () const {

        work::compact target {work::difficulty {Difficulty}};
        if (!target.valid ()) throw data::exception {} << "could not read difficulty " << Difficulty;

        // Category has no particular meaning. We could use it for
        // something like magic number if we wanted to imitate 21e8.
        int32_little category {Category ? *Category : 0};

        // User nonce is for ensuring that no two scripts are identical.
        // You can increase the bounty for a boost by making an identical script.
        uint32_little user_nonce {UserNonce ? *UserNonce : BoostPOW::casual_random {}.uint32 ()};

        // we are using version 1 for now.
        // we will use version 2 when we know we have Stratum extensions right.

        // This has to do with whether we use boost v2 which
        // incorporates bip320 which is necessary for ASICBoost.
        // This is not necessary for CPU mining.
        bool use_general_purpose_bits = Version == 2;

        // If you use a bounty script, other people can
        // compete with you to mine a boost output if you
        // broadcast it before you broadcast the solution.

        // If you use a contract script, then you are the only
        // one who can mine that boost output.

        return MinerPubkeyHash ?
            Boost::output_script::contract (
                category, Content, target,
                bytes (Topic), user_nonce,
                bytes (Data), *MinerPubkeyHash,
                use_general_purpose_bits) :
            Boost::output_script::bounty (
                category, Content, target,
                bytes (Topic), user_nonce,
                bytes (Data),
                use_general_purpose_bits);

    }
/*
    int run_spend (const argh::parser &command_line, int (*spend) (const script_options &)) {
        return spend (script_options::read (command_line));
    }

    void read_redeem_options (redeeming_options &options, const argh::parser &command_line, int secret_position, int address_position) {

        string secret_string;
        if (auto positional = command_line (secret_position); positional) positional >> secret_string;
        else if (auto option_wif = command_line ("wif"); option_wif) option_wif >> secret_string;
        else if (auto option_secret = command_line ("key"); option_secret) option_secret >> secret_string;
        else throw data::exception {"secret key not provided"};

        ptr<key_source> signing_keys;

        Bitcoin::secret key {secret_string};
        HD::BIP_32::secret hd_key {secret_string};

        if (key.valid ()) signing_keys =
            std::static_pointer_cast<key_source> (std::make_shared<single_key_source> (key));
        else if (hd_key.valid ()) signing_keys =
            std::static_pointer_cast<key_source> (std::make_shared<HD::key_source> (hd_key));
        else throw data::exception {"could not read signing key"};

        options.SigningKeys = std::make_shared<map_key_database> (signing_keys, 10);

        string address_string;
        if (auto positional = command_line (address_position); positional) positional >> address_string;
        else if (auto option = command_line ("address"); option) option >> address_string;

        if (address_string == "") {
            if (key.valid ()) options.ReceivingAddresses =
                std::static_pointer_cast<address_source> (std::make_shared<single_address_source> (key.address ()));
            else if (hd_key.valid ()) options.ReceivingAddresses =
                std::static_pointer_cast<address_source> (std::make_shared<HD::address_source> (hd_key.to_public ()));
            else throw data::exception {"could not read receiving address"};
        } else {
            Bitcoin::address::decoded address {address_string};
            HD::BIP_32::pubkey hd_pubkey {address_string};

            if (address.valid ()) options.ReceivingAddresses =
                std::static_pointer_cast<address_source> (std::make_shared<single_address_source> (address));
            else if (hd_pubkey.valid ()) options.ReceivingAddresses =
                std::static_pointer_cast<address_source> (std::make_shared<HD::address_source> (hd_pubkey));
            else throw data::exception {"could not read receiving address"};
        }

        if (auto option = command_line ("threads"); option) option >> options.Threads;
        if (options.Threads == 0) throw data::exception {"need at least one thread"};

        double fee_rate;
        if (auto option = command_line ("fee_rate"); option) {
            option >> fee_rate;
            options.FeeRate = fee_rate;
        }

        if (auto option = command_line ("api_endpoint"); option) options.APIHost = option.str ();

    }

    maybe<bytes> read_output_script (const string &script_string) {

        maybe<bytes> script_from_hex = encoding::hex::read (script_string);
        typed_data script_from_bip_276 = typed_data::read (script_string);

        Boost::output_script read_script {};

        if (script_from_bip_276.valid ()) {
            if (Boost::output_script::read (script_from_bip_276.Data).valid ())
                return script_from_bip_276.Data;
            else return {};
        } else if (bool (script_from_hex)) {
            if (Boost::output_script::read (*script_from_hex).valid ())
                return script_from_hex;
            else return {};
        }

        return {};

    }

    int64 read_satoshi_value (const argh::parser &command_line, int arg_position) {

        int64 satoshi_value = -1;
        if (auto positional = command_line (arg_position); positional) positional >> satoshi_value;
        else if (auto option = command_line ("value"); option) option >> satoshi_value;
        else return satoshi_value;

        if (satoshi_value <= 0) throw data::exception {} << "invalid satoshi value " << satoshi_value << " provided";
        return satoshi_value;
    }

    int run_redeem (const argh::parser &command_line,
        int (*redeem) (const Bitcoin::outpoint &, const bytes &script, int64_t value, const redeeming_options &)) {

        Bitcoin::outpoint outpoint {};
        maybe<bytes> boost_script {};

        string first_arg;
        if (auto positional = command_line (2); positional) {
            positional >> first_arg;

            // Test another way to read inputs for backwards compatibilty.
            // In an earlier version of the program, the boost script came first.
            boost_script = read_output_script (first_arg);
            if (bool (boost_script)) {

                // since a boost script was provided, we look to the next argument to be a satoshi value.
                int64 sats = read_satoshi_value (command_line, 3);

                string txid_string;
                if (auto positional = command_line (4); positional) positional >> txid_string;
                else if (auto option = command_line ("txid"); option) option >> txid_string;
                else throw data::exception {"option txid not provided "};

                outpoint.Digest = Bitcoin::txid {txid_string};
                if (!outpoint.Digest.valid ()) throw data::exception {} << "could not read txid " << txid_string;

                if (auto positional = command_line (5); positional) positional >> outpoint.Index;
                else if (auto option = command_line ("index"); option) option >> outpoint.Index;

                redeeming_options r {};

                read_redeem_options (r, command_line, 6, 7);

                return redeem (outpoint, *boost_script, sats, r);
            }

        } else if (auto option = command_line ("txid"); option) option >> first_arg;
        else throw data::exception {"option txid not provided "};

        // continue to read inputs normally.
        outpoint.Digest = Bitcoin::txid {first_arg};
        if (!outpoint.Digest.valid ()) throw data::exception {} << "could not read txid " << first_arg;

        if (auto positional = command_line (3); positional) positional >> outpoint.Index;
        else if (auto option = command_line ("index"); option) option >> outpoint.Index;

        redeeming_options options {};

        read_redeem_options (options, command_line, 4, 7);

        {
            string script_string;
            if (auto positional = command_line (5); positional) positional >> script_string;
            else if (auto option = command_line ("script"); option) option >> script_string;
            else goto no_script_provided;

            boost_script = read_output_script (script_string);
            if (!bool (boost_script)) throw data::exception {"could not read script"};
        }

        no_script_provided:

        int64 satoshi_value = read_satoshi_value (command_line, 6);

        return redeem (outpoint, *boost_script, satoshi_value, options);

    }

    int run_mine (const argh::parser &command_line,
        int (*mine) (const mining_options &)) {

        mining_options opts {};

        if (auto option = command_line ("min_profitability"); option) option >> opts.MinProfitability;
        if (auto option = command_line ("min_value"); option) option >> opts.MinValue;
        if (auto option = command_line ("max_difficulty"); option) option >> opts.MaxDifficulty;
        if (auto option = command_line ("refresh_interval"); option) option >> opts.RefreshInterval;
        opts.Websockets = command_line["websockets"];

        read_redeem_options (opts, command_line, 2, 3);

        return mine (opts);
    }*/

}
