#include <Cosmos/boost/miner_options.hpp>
#include <data/random.hpp>
#include <argh.h>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/schema/hd.hpp>

namespace BoostPOW {

    script_options script_options::read (const args &command_line, int start_pos) {

        script_options options;

        maybe<std::string> content_hex;
        if (command_line.get (start_pos, content_hex); content_hex) options.Content = digest256 {*content_hex};
        else if (command_line.get ("content", content_hex); content_hex) options.Content = digest256 {*content_hex};
        else throw data::exception {"option content not provided "};
        if (!options.Content.valid ()) throw data::exception {} << "invalid content string: " << *content_hex;

        maybe<double> difficulty;
        if (command_line.get (start_pos + 1, difficulty); difficulty) options.Difficulty = *difficulty;
        else if (command_line.get ("difficulty", difficulty); difficulty) options.Difficulty = *difficulty;
        else throw data::exception {"option difficulty not provided "};

        if (options.Difficulty <= 0) throw data::exception {} << "difficulty must be > 0; value provided was " << options.Difficulty;

        maybe<std::string> topic;
        if (command_line.get (start_pos + 2, topic); topic) options.Topic = *topic;
        else if (command_line.get ("topic", topic); topic) options.Topic = *topic;

        maybe<std::string> data;
        if (command_line.get (start_pos + 3, data); data) options.Data = *data;
        else if (command_line.get ("data", data); data) options.Data = *data;

        maybe<std::string> address;

        command_line.get (start_pos + 4, address);
        if (!bool (address)) command_line.get ("address", address);
        else if (bool (address) && *address != "") {
            Bitcoin::address miner_address {*address};
            if (!miner_address.valid ()) throw data::exception {} << "invalid address provided: " << address;
            options.MinerPubkeyHash = miner_address.digest ();
        } else throw data::exception {} << "no address provided. ";

        maybe<uint32> version;
        if (command_line.get<uint32> ("version", version); version) options.Version = *version;

        if (options.Version < 1 || options.Version > 2) throw data::exception {} << "invalid script version " << options.Version;

        maybe<uint32> unonce;
        if (command_line.get<uint32> ("user_nonce", unonce); unonce)
            options.UserNonce = *unonce;

        maybe<int32> category;
        if (command_line.get<int32> ("category", category); category)
            options.Category = *category;

        return options;
    }

    script_options::operator output_script () const {

        namespace work = Gigamonkey::work;

        work::compact target {work::difficulty {Difficulty}};
        if (!target.valid ()) throw data::exception {} << "could not read difficulty " << Difficulty;

        // Category has no particular meaning. We could use it for
        // something like magic number if we wanted to imitate 21e8.
        int32_little category {Category ? *Category : 0};

        // User nonce is for ensuring that no two scripts are identical.
        // You can increase the bounty for a boost by making an identical script.
        uint32_little user_nonce {0};
        if (bool (UserNonce)) user_nonce = *UserNonce;
        else data::random::get () >> user_nonce;

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
            output_script::contract (
                category, Content, target,
                bytes (Topic), user_nonce,
                bytes (Data), *MinerPubkeyHash,
                use_general_purpose_bits) :
            output_script::bounty (
                category, Content, target,
                bytes (Topic), user_nonce,
                bytes (Data),
                use_general_purpose_bits);

    }

}
