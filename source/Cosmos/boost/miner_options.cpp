#include <Cosmos/boost/miner_options.hpp>
#include <Cosmos/random.hpp>
#include <argh.h>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/schema/hd.hpp>

namespace BoostPOW {

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
        uint32_little user_nonce {0};
        if (bool (UserNonce)) user_nonce = *UserNonce;
        else *Cosmos::get_casual_random () >> user_nonce;

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

}
