
#ifndef COSMOS_BOOST_PROGRAM_OPTIONS
#define COSMOS_BOOST_PROGRAM_OPTIONS

#include <string>
#include <optional>
#include <data/io/arg_parser.hpp>
#include <gigamonkey/boost/boost.hpp>
#include <Cosmos/types.hpp>

namespace BoostPOW {
    using namespace Gigamonkey::Boost;
    using args = data::io::args::parsed;

    struct script_options {
        // Content is what is to be boosted. Could be a hash or
        // could be text that's 32 bytes or less. There is a
        // BIG PROBLEM with the fact that hashes in Bitcoin are
        // often displayed reversed. This is a convention that
        // got started long ago because people were stupid.

        // For average users of boost, we need to ensure that
        // the hash they are trying to boost actually exists. We
        // should not let them paste in hashes to boost; we should
        // make them select content to be boosted.

        // In my library, we read the string backwards by putting
        // an 0x at the front.
        digest256 Content {};

        // difficulty is a unit that is inversely proportional to
        // target. One difficulty is proportional to 2^32
        // expected hash operations.

        // a difficulty of 1/1000 should be easy to do on a cpu quickly.
        // Difficulty 1 is the difficulty of the genesis block.
        double Difficulty {0};

        // Tag/topic does not need to be anything.
        string Topic {};

        // additional data does not need to be anything but it
        // can be used to provide information about a boost or
        // to add a comment.
        string Data {};

        // script version can be 1 or 2.
        uint32 Version {2};

        // if provided, a contract script will be created.
        // Otherwise a bounty script will be created.
        maybe<digest160> MinerPubkeyHash {};

        maybe<uint32> UserNonce {};
        maybe<int32> Category {};

        static script_options read (const args &, int start_pos = 2);
        explicit operator output_script () const;

        net::HTTP::request request (const UTF8 & = "localhost") {
            throw data::method::unimplemented {"http request from boost options"};
        }
    };
}

#endif
