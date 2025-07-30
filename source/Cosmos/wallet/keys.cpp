#include <Cosmos/wallet/change.hpp>
#include <gigamonkey/script/pattern/pay_to_pubkey.hpp>

#include <math.h>

namespace Cosmos {

    key_expression::operator secp256k1::secret () const {
        throw method::unimplemented {"key_expression::operator secp256k1::secret"};
    }

    key_expression::operator Bitcoin::secret () const {
        throw method::unimplemented {"key_expression::operator Bitcoin::secret"};
    }

    key_expression::operator HD::BIP_32::secret () const {
        throw method::unimplemented {"key_expression::operator HD::BIP_32::secret"};
    }

    key_expression::operator secp256k1::pubkey () const {
        throw method::unimplemented {"key_expression::operator secp256k1::pubkey"};
    }

    key_expression::operator Bitcoin::pubkey () const {
        throw method::unimplemented {"key_expression::operator Bitcoin::pubkey"};
    }

    key_expression::operator HD::BIP_32::pubkey () const {
        throw method::unimplemented {"key_expression::operator HD::BIP_32::pubkey"};
    }

}