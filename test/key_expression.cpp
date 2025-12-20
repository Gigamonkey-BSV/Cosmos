
#include <Cosmos/wallet/keys.hpp>
#include "gtest/gtest.h"

namespace Cosmos {

    template <typename ...K> struct test_cast_key;

    template <typename K> struct test_cast_key<K, K> {
        test_cast_key (const key_expression &k) {
            EXPECT_NO_THROW (K (k));
        }
    };

    template <typename X, typename K> struct test_cast_key<X, K> {
        test_cast_key (const key_expression &k) {
            try {
                K (k);
            } catch (data::exception &) {}
        }
    };

    template <typename X, typename K, typename ...Y>
    struct test_cast_key<X, K, Y...> : test_cast_key<X, K>, test_cast_key<X, Y...> {
        test_cast_key (const key_expression &k):
            test_cast_key<X, K> {k}, test_cast_key<X, Y...> {k} {}
    };

    template <typename X> struct test_read_key {
        test_read_key (const key_expression &k) {
            test_cast_key<X, secp256k1::secret, Bitcoin::secret, HD::BIP_32::secret, secp256k1::pubkey, Bitcoin::pubkey, HD::BIP_32::pubkey> {k};
        }
    };

    TEST (Keys, ReadKey) {
        // invalid key expression
        key_expression invalid {};

        key_expression private_key {secp256k1::secret {123}};

        key_expression private_key_string {"secret 123"};

        key_expression public_key {to_public (secp256k1::secret {123})};

        key_expression public_key_string {"pubkey `03cc45122542e88a92ea2e4266424a22e83292ff6a2bc17cdd7110f6d10fe32523`"};

        key_expression WIF {Bitcoin::WIF::encode (Bitcoin::net::Main, secp256k1::secret {123})};

        key_expression WIF_decoded {"WIF [secret 123]"};

        key_expression WIF_encoded {R"(WIF "L1LokMeMLVbnapboYCpeobZ67FkFBXKhYLMPs9mj7X4vk58AdCZQ")"};

        key_expression HD_private {};

        key_expression HD_pubkey {};

        key_expression HD_private_encoded {"HD.secret [secret 123, 'c15223e9f5e99e43aa4162ecc0eba20b003297eba1b5052be95f257034ec4b77']"};

        key_expression HD_private_decoded {
            R"(HD.secret "xprv9s21ZrQH143K3yzPZD4Qe6M6hbPVZPrsARe7T1Ly9tJHj7jKFmVreHERV7A9eBZZsB5fzefvChhro43yQgXwAzPpJ9nC9SUWwXnFEa8m4km")"};

        key_expression HD_pubkey_encoded {"HD.pubkey [(pubkey '03cc45122542e88a92ea2e4266424a22e83292ff6a2bc17cdd7110f6d10fe32523'), 'c15223e9f5e99e43aa4162ecc0eba20b003297eba1b5052be95f257034ec4b77', net.Main]"};

        key_expression HD_pubkey_decoded {R"(HD.pubkey "xpub661MyMwAqRbcGU4rfEbR1EHqFdDyxraiXeZiFPkaiDqGbv4ToJp7C5YuLQkTBtRxL8oqjPHwWJuXUTENqLF2i7j8Bit8HqKLhEzcm6uZPPA")"};

        test_read_key<int> {invalid};

        EXPECT_EQ (private_key, private_key_string);

        test_read_key<secp256k1::secret> {private_key};
        test_read_key<secp256k1::pubkey> {private_key};
        test_read_key<Bitcoin::pubkey> {private_key};

        test_read_key<secp256k1::secret> {private_key_string};
        test_read_key<secp256k1::pubkey> {private_key_string};
        test_read_key<Bitcoin::pubkey> {private_key_string};

        EXPECT_EQ (public_key, public_key_string);

        test_read_key<secp256k1::pubkey> {public_key};
        test_read_key<Bitcoin::pubkey> {public_key};

        test_read_key<secp256k1::pubkey> {public_key_string};
        test_read_key<Bitcoin::pubkey> {public_key_string};

        test_read_key<secp256k1::secret> {WIF};
        test_read_key<Bitcoin::secret> {WIF};
        test_read_key<Bitcoin::WIF> {WIF};
        test_read_key<secp256k1::pubkey> {WIF};
        test_read_key<Bitcoin::pubkey> {WIF};

        test_read_key<secp256k1::secret> {WIF_decoded};
        test_read_key<Bitcoin::secret> {WIF_decoded};
        test_read_key<Bitcoin::WIF> {WIF_decoded};
        test_read_key<secp256k1::pubkey> {WIF_decoded};
        test_read_key<Bitcoin::pubkey> {WIF_decoded};

        test_read_key<secp256k1::secret> {WIF_encoded};
        test_read_key<Bitcoin::secret> {WIF_encoded};
        test_read_key<Bitcoin::WIF> {WIF_encoded};
        test_read_key<secp256k1::pubkey> {WIF_encoded};
        test_read_key<Bitcoin::pubkey> {WIF_encoded};

        test_read_key<secp256k1::secret> {HD_private};
        test_read_key<Bitcoin::secret> {HD_private};
        test_read_key<Bitcoin::WIF> {HD_private};
        test_read_key<secp256k1::pubkey> {HD_private};
        test_read_key<Bitcoin::pubkey> {HD_private};
        test_read_key<HD::BIP_32::secret> {HD_private};
        test_read_key<HD::BIP_32::pubkey> {HD_private};

        test_read_key<secp256k1::secret> {HD_private_encoded};
        test_read_key<Bitcoin::secret> {HD_private_encoded};
        test_read_key<Bitcoin::WIF> {HD_private_encoded};
        test_read_key<secp256k1::pubkey> {HD_private_encoded};
        test_read_key<Bitcoin::pubkey> {HD_private_encoded};
        test_read_key<HD::BIP_32::secret> {HD_private_encoded};
        test_read_key<HD::BIP_32::pubkey> {HD_private_encoded};

        test_read_key<secp256k1::secret> {HD_private_decoded};
        test_read_key<Bitcoin::secret> {HD_private_decoded};
        test_read_key<Bitcoin::WIF> {HD_private_decoded};
        test_read_key<secp256k1::pubkey> {HD_private_decoded};
        test_read_key<Bitcoin::pubkey> {HD_private_decoded};
        test_read_key<HD::BIP_32::secret> {HD_private_decoded};
        test_read_key<HD::BIP_32::pubkey> {HD_private_decoded};

        test_read_key<secp256k1::pubkey> {HD_pubkey};
        test_read_key<Bitcoin::pubkey> {HD_pubkey};
        test_read_key<HD::BIP_32::pubkey> {HD_pubkey};

        test_read_key<secp256k1::pubkey> {HD_pubkey_encoded};
        test_read_key<Bitcoin::pubkey> {HD_pubkey_encoded};
        test_read_key<HD::BIP_32::pubkey> {HD_pubkey_encoded};

        test_read_key<secp256k1::pubkey> {HD_pubkey_decoded};
        test_read_key<Bitcoin::pubkey> {HD_pubkey_decoded};
        test_read_key<HD::BIP_32::pubkey> {HD_pubkey_decoded};





    }

}
