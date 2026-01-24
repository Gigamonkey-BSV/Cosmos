#include <Cosmos/wallet/keys.hpp>
#include <Cosmos/Diophant.hpp>
#include <data/encoding/read.hpp>
#include <regex>

namespace Cosmos {

    std::string inline net_string (Bitcoin::net net) {
        if (net == Bitcoin::net::Test) return "net.Test";
        if (net == Bitcoin::net::Main) return "net.Main";
        throw data::exception {} << "invalid network";
    }

    std::ostream &write_Bitcoin_secret (std::ostream &o, const Bitcoin::secret &x) {
        o << "WIF [secret " << std::dec << x.Secret.Value;
        if (x.Network != Bitcoin::net::Main || !x.Compressed) o << ", " << net_string (x.Network);
        if (!x.Compressed) o << ", false";
        return o << "]";
    }

    std::ostream &write_HD_secret (std::ostream &o, const HD::BIP_32::secret &x) {
        o << "HD.secret [secret " << std::dec << x.Secret.Value << ", `" << encoding::hex::write (x.ChainCode) << "`";
        if (x.Network != Bitcoin::net::Main || x.Depth != 0 || x.Parent != 0 || x.Sequence != 0) o << ", " << net_string (x.Network);
        if (x.Depth != 0 || x.Parent != 0 || x.Sequence != 0) o << ", " << uint32 (x.Depth);
        if (x.Parent != 0 || x.Sequence != 0) o << ", " << x.Parent;
        if (x.Sequence != 0) o << ", " << x.Parent;
        return o << "]";
    }

    std::ostream &write_HD_pubkey (std::ostream &o, const HD::BIP_32::pubkey &x) {
        o << "HD.pubkey [pubkey `" << encoding::hex::write (x.Pubkey) << "`, `" << encoding::hex::write (x.ChainCode) << "`";
        if (x.Network != Bitcoin::net::Main || x.Depth != 0 || x.Parent != 0 || x.Sequence != 0) o << ", " << net_string (x.Network);
        if (x.Depth != 0 || x.Parent != 0 || x.Sequence != 0) o << ", " << uint32 (x.Depth);
        if (x.Parent != 0 || x.Sequence != 0) o << ", " << x.Parent;
        if (x.Sequence != 0) o << ", " << x.Parent;
        return o << "]";
    }

    template <typename F, typename X> string write_key (F &&f, const X &x) {
        std::stringstream ss;
        f (ss, x);
        return ss.str ();
    }

    key_expression::key_expression (const Bitcoin::secret &x): string {write_key (&write_Bitcoin_secret, x)} {}
    key_expression::key_expression (const HD::BIP_32::secret &x): string {write_key (&write_HD_secret, x)} {}
    key_expression::key_expression (const HD::BIP_32::pubkey &x): string {write_key (&write_HD_pubkey, x)} {}

    using string_view = data::string_view;

    using extracted = list<string_view>;

    extracted extract (const std::regex &re, string_view text) {
        extracted result {};

        std::match_results<std::string_view::const_iterator> match {};

        if (std::regex_search (text.begin (), text.end (), match, re)) {

            size_t matches = match.size ();

            for (size_t i = 1; i < matches; i++) if (match[i].matched) {
                auto begin = match[i].first;
                auto end   = match[i].second;
                result <<= string_view {begin, static_cast<size_t> (end - begin)};
            }
        }

        return result;
    }

    const std::regex Bitcoin_address_string (R"REGEX(^\s*address\s*`[1-9A-HJ-NP-Za-km-z]+`\s*$)REGEX");
    const std::regex Bitcoin_address_decoded (R"REGEX(^\s*address\s*\[\s*`([0-9A-Fa-f]+)`\s*(?:,\s*net\.(Test|Main)\s*)?\]\s*$)REGEX");

    const std::regex secp256k1_secret (R"REGEX(^\s*secret\s+(\d+)\s*$)REGEX");
    const std::regex secp256k1_pubkey (R"REGEX(^\s*pubkey\s+`([0-9A-Fa-f]+)`\s*$)REGEX");
    const std::regex WIF_string (R"REGEX(^\s*WIF\s+"([1-9A-HJ-NP-Za-km-z]+)"\s*$)REGEX");
    const std::regex HD_secret_string (R"REGEX(^\s*HD\s*\.\s*secret\s+"([1-9A-HJ-NP-Za-km-z]+)"\s*$)REGEX");
    const std::regex HD_pubkey_string (R"REGEX(^\s*HD\s*\.\s*pubkey\s+"([1-9A-HJ-NP-Za-km-z]+)"\s*$)REGEX");
    const std::regex WIF_decoded (R"REGEX(^\s*WIF\s+\[\s*secret\s+(\d+)\s*(?:,\s*net\.(Test|Main)\s*(?:,\s*(true|false)\s*)?)?\]\s*$)REGEX");

    const std::regex HD_secret_decoded
        (R"REGEX(^\s*HD\s*\.\s*secret\s+\[\s*secret\s+(\d+)\s*,\s*`([\dA-Fa-f]+)`\s*(?:,\s*net\.(Test|Main)\s*(?:,\s*(\d+)\s*(?:,\s*(\d+)\s*(?:,\s*(\d+)\s*)?)?)?)?\]\s*$)REGEX");

    const std::regex HD_pubkey_decoded
        (R"REGEX(^\s*HD\s*\.\s*pubkey\s+\[\s*pubkey\s+`([\dA-Fa-f]+)`\s*,\s*`([\dA-Fa-f]+)`\s*(?:,\s*net\.(Test|Main)\s*(?:,\s*(\d+)\s*(?:,\s*(\d+)\s*(?:,\s*(\d+)\s*)?)?)?)?\]\s*$)REGEX");

    extracted inline extract_secp256k1_secret (string_view x) {
        return extract (secp256k1_secret, x);
    }

    extracted inline extract_secp256k1_pubkey (string_view x) {
        return extract (secp256k1_pubkey, x);
    }

    extracted inline extract_WIF_string (string_view x) {
        return extract (WIF_string, x);
    }

    extracted inline extract_WIF_decoded (string_view x) {
        return extract (WIF_decoded, x);
    }

    extracted inline extract_HD_secret_string (string_view x) {
        return extract (HD_secret_string, x);
    }

    extracted inline extract_HD_pubkey_string (string_view x) {
        return extract (HD_pubkey_string, x);
    }

    extracted inline extract_HD_secret_decoded (string_view x) {
        return extract (HD_secret_decoded, x);
    }

    extracted inline extract_HD_pubkey_decoded (string_view x) {
        return extract (HD_pubkey_decoded, x);
    }

    extracted inline extract_Bitcoin_address_string (string_view x) {
        return extract (Bitcoin_address_string, x);
    }

    extracted inline extract_Bitcoin_address_decoded (string_view x) {
        return extract (Bitcoin_address_decoded, x);
    }

    // TODO technically, we should look at the type of the
    // expression to determine whether it's a key but we
    // can't yet.
    bool key_expression::valid () const {
        return bool (!extract_secp256k1_secret (*this).empty ()) ||
            bool (!extract_secp256k1_pubkey (*this).empty ()) ||
            bool (!extract_WIF_string (*this).empty ()) ||
            bool (!extract_WIF_decoded (*this).empty ()) ||
            bool (!extract_HD_pubkey_string (*this).empty ()) ||
            bool (!extract_HD_secret_string (*this).empty ()) ||
            bool (!extract_HD_pubkey_decoded (*this).empty ()) ||
            bool (!extract_HD_secret_decoded (*this).empty ()) ||
            bool (!extract_Bitcoin_address_string (*this).empty ()) ||
            bool (!extract_Bitcoin_address_decoded (*this).empty ());
    }

    secp256k1::secret inline read_secp256k1_secret (string_view b) {
        return secp256k1::secret {Gigamonkey::uint256 (b)};
    }

    Bitcoin::pubkey inline read_Bitcoin_pubkey (string_view b) {
        return Bitcoin::pubkey (b);
    }

    Bitcoin::net inline read_net (string_view b) {
        return b.size () > 0 && b['M'] ? Bitcoin::net::Main : Bitcoin::net::Test;
    }

    bool inline read_bool (string_view b) {
        return b.size () == 4;
    }

    secp256k1::secret inline make_secp256k1_secret (extracted x) {
        return read_secp256k1_secret (x[0]);
    }

    Bitcoin::pubkey inline make_Bitcoin_pubkey (extracted x) {
        return read_Bitcoin_pubkey (x[0]);
    }

    Bitcoin::WIF inline make_WIF_string (extracted x) {
        return Bitcoin::WIF (x[0]);
    }

    Bitcoin::secret make_Bitcoin_secret (extracted x) {
        Bitcoin::net net = x.size () > 1 ? read_net (x[1]) : Bitcoin::net::Main;
        bool compressed = x.size () > 2 ? read_bool (x[2]) : true;
        return Bitcoin::secret (net, read_secp256k1_secret (x[0]), compressed);
    }

    HD::BIP_32::pubkey inline make_HD_pubkey_string (extracted x) {
        return HD::BIP_32::pubkey (x[0]);
    }

    HD::BIP_32::secret inline make_HD_secret_string (extracted x) {
        return HD::BIP_32::secret (x[0]);
    }

    HD::BIP_32::pubkey make_HD_pubkey_decoded (extracted x) {
        Bitcoin::net net = x.size () > 2 ? read_net (x[2]) : Bitcoin::net::Main;
        byte depth = x.size () > 3 ? *data::encoding::read<byte> {} (x[3]) : 0;
        uint32 parent = x.size () > 4 ? *data::encoding::read<uint32> {} (x[4]) : 0;
        uint32 sequence = x.size () > 5 ? *data::encoding::read<uint32> {} (x[5]) : 0;
        return HD::BIP_32::pubkey {read_Bitcoin_pubkey (x[0]), HD::chain_code {x[1]}, net, depth, parent, sequence};
    }

    HD::BIP_32::secret make_HD_secret_decoded (extracted x) {
        Bitcoin::net net = x.size () > 2 ? read_net (x[2]) : Bitcoin::net::Main;
        byte depth = x.size () > 3 ? *data::encoding::read<byte> {} (x[3]) : 0;
        uint32 parent = x.size () > 4 ? *data::encoding::read<uint32> {} (x[4]) : 0;
        uint32 sequence = x.size () > 5 ? *data::encoding::read<uint32> {} (x[5]) : 0;
        return HD::BIP_32::secret {read_secp256k1_secret (x[0]), HD::chain_code {x[1]}, net, depth, parent, sequence};
    }

    Bitcoin::address inline make_Bitcoin_address_string (extracted x) {
        return Bitcoin::address (x[0]);
    }

    Bitcoin::address::decoded inline make_Bitcoin_address_decoded (extracted x) {
        Bitcoin::net net = x.size () > 1 ? read_net (x[1]) : Bitcoin::net::Main;
        return Bitcoin::address::decoded (net, Gigamonkey::digest160 {x[0]});
    }

    key_expression::operator Bitcoin::address () const {
        if (auto x = extract_Bitcoin_address_string (*this); !x.empty ())
            return make_Bitcoin_address_string (x);
        return operator Bitcoin::address::decoded ().encode ();
    }

    key_expression::operator Bitcoin::address::decoded () const {
        if (auto x = extract_Bitcoin_address_string (*this); !x.empty ())
            return make_Bitcoin_address_string (x).decode ();
        if (auto x = extract_Bitcoin_address_decoded (*this); !x.empty ())
            return make_Bitcoin_address_decoded (x);
        if (auto x = extract_secp256k1_pubkey (*this); !x.empty ())
            return Bitcoin::address::decoded {Bitcoin::net::Main, make_Bitcoin_pubkey (x).address_hash ()};
        if (auto x = extract_HD_pubkey_string (*this); !x.empty ())
            return make_HD_pubkey_string (x).address ();
        if (auto x = extract_HD_pubkey_decoded (*this); !x.empty ())
            return make_HD_pubkey_decoded (x).address ();
        if (auto x = extract_secp256k1_secret (*this); !x.empty ())
            return Bitcoin::address::decoded {Bitcoin::net::Main, Bitcoin::pubkey {make_secp256k1_secret (x).to_public ()}.address_hash ()};
        if (auto x = extract_WIF_string (*this); !x.empty ())
            return make_WIF_string (x).decode ().address ();
        if (auto x = extract_WIF_decoded (*this); !x.empty ())
            return make_Bitcoin_secret (x).address ();
        if (auto x = extract_HD_secret_string (*this); !x.empty ())
            return make_HD_secret_string (x).to_public ().address ();
        if (auto x = extract_HD_secret_decoded (*this); !x.empty ())
            return make_HD_secret_decoded (x).to_public ().address ();
        throw data::exception {} << "invalid key expression";
    }

    key_expression::operator Bitcoin::pubkey () const {
        if (auto x = extract_secp256k1_pubkey (*this); !x.empty ())
            return make_Bitcoin_pubkey (x);
        if (auto x = extract_HD_pubkey_string (*this); !x.empty ())
            return make_HD_pubkey_string (x).Pubkey;
        if (auto x = extract_HD_pubkey_decoded (*this); !x.empty ())
            return make_HD_pubkey_decoded (x).Pubkey;
        if (auto x = extract_secp256k1_secret (*this); !x.empty ())
            return make_secp256k1_secret (x).to_public ();
        if (auto x = extract_WIF_string (*this); !x.empty ())
            return make_WIF_string (x).secret ().to_public ();
        if (auto x = extract_WIF_decoded (*this); !x.empty ())
            return make_Bitcoin_secret (x).to_public ();
        if (auto x = extract_HD_secret_string (*this); !x.empty ())
            return make_HD_secret_string (x).Secret.to_public ();
        if (auto x = extract_HD_secret_decoded (*this); !x.empty ())
            return make_HD_secret_decoded (x).Secret.to_public ();
        throw data::exception {} << "invalid key expression";
    }

    key_expression::operator secp256k1::pubkey () const {
        return operator Bitcoin::pubkey ();
    }

    key_expression::operator HD::BIP_32::pubkey () const {
        if (auto x = extract_HD_pubkey_string (*this); !x.empty ())
            return make_HD_pubkey_string (x);
        if (auto x = extract_HD_pubkey_decoded (*this); !x.empty ())
            return make_HD_pubkey_decoded (x);
        if (auto x = extract_HD_secret_string (*this); !x.empty ())
            return make_HD_secret_string (x).to_public ();
        if (auto x = extract_HD_secret_decoded (*this); !x.empty ())
            return make_HD_secret_decoded (x).to_public ();
        throw data::exception {} << "invalid key expression";
    }

    key_expression::operator secp256k1::secret () const {
        if (auto x = extract_secp256k1_secret (*this); !x.empty ())
            return make_secp256k1_secret (x);
        if (auto x = extract_WIF_string (*this); !x.empty ())
            return make_WIF_string (x).secret ();
        if (auto x = extract_WIF_decoded (*this); !x.empty ())
            return make_Bitcoin_secret (x).Secret;
        if (auto x = extract_HD_secret_string (*this); !x.empty ())
            return make_HD_secret_string (x).Secret;
        if (auto x = extract_HD_secret_decoded (*this); !x.empty ())
            return make_HD_secret_decoded (x).Secret;
        throw data::exception {} << "invalid key expression";
    }

    key_expression::operator Bitcoin::secret () const {
        if (auto x = extract_WIF_string (*this); !x.empty ())
            return make_WIF_string (x).decode ();
        if (auto x = extract_WIF_decoded (*this); !x.empty ())
            return make_Bitcoin_secret (x);
        if (auto x = extract_HD_secret_string (*this); !x.empty ())
            return static_cast<Bitcoin::secret> (make_HD_secret_string (x));
        if (auto x = extract_HD_secret_decoded (*this); !x.empty ())
            return static_cast<Bitcoin::secret> (make_HD_secret_decoded (x));
        throw data::exception {} << "invalid key expression";
    }

    key_expression::operator Bitcoin::WIF () const {
        if (auto x = extract_WIF_string (*this); !x.empty ())
            return make_WIF_string (x);
        if (auto x = extract_WIF_decoded (*this); !x.empty ())
            return make_Bitcoin_secret (x).encode ();
        if (auto x = extract_HD_secret_string (*this); !x.empty ())
            return static_cast<Bitcoin::secret> (make_HD_secret_string (x)).encode ();
        if (auto x = extract_HD_secret_decoded (*this); !x.empty ())
            return static_cast<Bitcoin::secret> (make_HD_secret_decoded (x)).encode ();
        throw data::exception {} << "invalid key expression";
    }

    key_expression::operator HD::BIP_32::secret () const {
        if (auto x = extract_HD_secret_string (*this); !x.empty ())
            return make_HD_secret_string (x);
        if (auto x = extract_HD_secret_decoded (*this); !x.empty ())
            return make_HD_secret_decoded (x);
        throw data::exception {} << "invalid key expression";
    }

    key_expression key_expression::to_public () const {
        if (auto x = extract_secp256k1_secret (*this); !x.empty ())
            return make_secp256k1_secret (x).to_public ();
        if (auto x = extract_WIF_string (*this); !x.empty ())
            return make_WIF_string (x).decode ().to_public ();
        if (auto x = extract_WIF_decoded (*this); !x.empty ())
            return make_Bitcoin_secret (x).to_public ();
        if (auto x = extract_HD_secret_string (*this); !x.empty ())
            return make_HD_secret_string (x).to_public ();
        if (auto x = extract_HD_secret_decoded (*this); !x.empty ())
            return make_HD_secret_decoded (x).to_public ();
        throw data::exception {} << "invalid key expression";
    }

}
