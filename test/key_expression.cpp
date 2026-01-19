
#include <Cosmos/wallet/keys.hpp>
#include "gtest/gtest.h"

namespace Cosmos {

    template <typename ...K> struct test_cast_key;

    template <typename ...K> struct keys;

    template <typename K> struct test_cast_key<keys<>, K> {
        test_cast_key (const key_expression &k) {
            try {
                k.operator K ();
                [] () {
                    FAIL () << "Failed to throw exception";
                } ();
            } catch (data::exception &) {}
        }
    };

    template <typename K, typename ...X> struct test_cast_key<keys<K, X...>, K> {
        test_cast_key (const key_expression &k) {
            EXPECT_NO_THROW (k.operator K ());
        }
    };

    template <typename K, typename L, typename ...X> struct test_cast_key<keys<L, X...>, K> : test_cast_key<keys<X...>, K> {
        test_cast_key (const key_expression &k) : test_cast_key<keys<X...>, K> {k} {}
    };

    template <typename X, typename Y> struct test_cast_keys;

    template <typename ...K, typename ...L>
    struct test_cast_keys<keys<K...>, keys<L...>> {
        test_cast_keys (const key_expression &k) {
            ((void)(test_cast_key<keys<K...>, L> {k}), ...);
        }
    };

    template <typename ...X> struct test_read_key {
        test_read_key (const key_expression &k) {
            test_cast_keys<keys<X...>,
                keys<Bitcoin::address, Bitcoin::address::decoded,
                    secp256k1::secret, Bitcoin::secret,
                    secp256k1::pubkey, Bitcoin::pubkey,
                    HD::BIP_32::secret, HD::BIP_32::pubkey>> {k};
        }
    };

    TEST (Keys, ReadKey) {

        key_expression private_key {secp256k1::secret {123}};

        key_expression private_key_string {"secret 123"};

        key_expression public_key {to_public (secp256k1::secret {123})};

        key_expression public_key_string {"pubkey `03cc45122542e88a92ea2e4266424a22e83292ff6a2bc17cdd7110f6d10fe32523`"};

        key_expression WIF {Bitcoin::WIF::encode (Bitcoin::net::Main, secp256k1::secret {123})};

        key_expression WIF_decoded {"WIF [secret 123]"};

        key_expression WIF_encoded {R"(WIF "L1LokMeMLVbnapboYCpeobZ67FkFBXKhYLMPs9mj7X4vk58AdCZQ")"};

        key_expression HD_private {HD::BIP_32::secret {secp256k1::secret {123}, Bitcoin::Hash256 ("Hiya buddy")}};

        key_expression HD_pubkey {HD::BIP_32::secret {secp256k1::secret {123}, Bitcoin::Hash256 ("Hiya buddy")}.to_public ()};

        key_expression HD_private_encoded {"HD.secret [secret 123, `c15223e9f5e99e43aa4162ecc0eba20b003297eba1b5052be95f257034ec4b77`]"};

        key_expression HD_private_decoded {
            R"(HD.secret "xprv9s21ZrQH143K3yzPZD4Qe6M6hbPVZPrsARe7T1Ly9tJHj7jKFmVreHERV7A9eBZZsB5fzefvChhro43yQgXwAzPpJ9nC9SUWwXnFEa8m4km")"};

        key_expression HD_pubkey_encoded {"HD.pubkey [pubkey `03cc45122542e88a92ea2e4266424a22e83292ff6a2bc17cdd7110f6d10fe32523`, `c15223e9f5e99e43aa4162ecc0eba20b003297eba1b5052be95f257034ec4b77`, net.Main]"};

        key_expression HD_pubkey_decoded {R"(HD.pubkey "xpub661MyMwAqRbcGU4rfEbR1EHqFdDyxraiXeZiFPkaiDqGbv4ToJp7C5YuLQkTBtRxL8oqjPHwWJuXUTENqLF2i7j8Bit8HqKLhEzcm6uZPPA")"};

        EXPECT_EQ (private_key, private_key_string);

        test_read_key<secp256k1::secret, secp256k1::pubkey, Bitcoin::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {private_key};

        test_read_key<secp256k1::secret, secp256k1::pubkey, Bitcoin::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {private_key_string};

        EXPECT_EQ (public_key, public_key_string);

        test_read_key<secp256k1::pubkey, Bitcoin::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {public_key};

        test_read_key<secp256k1::pubkey, Bitcoin::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {public_key_string};

        test_read_key<secp256k1::secret, Bitcoin::secret, Bitcoin::WIF, secp256k1::pubkey, Bitcoin::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {WIF};

        test_read_key<secp256k1::secret, Bitcoin::secret, Bitcoin::WIF, secp256k1::pubkey, Bitcoin::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {WIF_decoded};

        test_read_key<secp256k1::secret, Bitcoin::secret, Bitcoin::WIF, secp256k1::pubkey, Bitcoin::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {WIF_encoded};

        test_read_key<secp256k1::secret, Bitcoin::secret, Bitcoin::WIF, secp256k1::pubkey, Bitcoin::pubkey,
            HD::BIP_32::secret, HD::BIP_32::pubkey, Bitcoin::address, Bitcoin::address::decoded>
            {HD_private_encoded};

        test_read_key<secp256k1::secret, Bitcoin::secret, Bitcoin::WIF, secp256k1::pubkey, Bitcoin::pubkey,
            HD::BIP_32::secret, HD::BIP_32::pubkey, Bitcoin::address, Bitcoin::address::decoded>
            {HD_private_decoded};

        test_read_key<secp256k1::pubkey, Bitcoin::pubkey, HD::BIP_32::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {HD_pubkey_encoded};

        test_read_key<secp256k1::pubkey, Bitcoin::pubkey, HD::BIP_32::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {HD_pubkey_decoded};

        test_read_key<secp256k1::secret, Bitcoin::secret, Bitcoin::WIF, secp256k1::pubkey, Bitcoin::pubkey,
            HD::BIP_32::secret, HD::BIP_32::pubkey, Bitcoin::address, Bitcoin::address::decoded>
            {HD_private};

        test_read_key<secp256k1::pubkey, Bitcoin::pubkey, HD::BIP_32::pubkey,
            Bitcoin::address, Bitcoin::address::decoded> {HD_pubkey};

    }

}
