#include <Cosmos/Diophant.hpp>

namespace Cosmos {

    maybe<Diophant::machine> machine {};

    void initialize (ptr<database>) {
        if (bool (machine)) return;
        machine = Diophant::initialize ();

        // TODO set up functions
        // * key
        // * to_private
        // * invert_hash

        //throw data::method::unimplemented {"initialize"};
    }

    key_expression to_private (const key_expression &) {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"to_private"};
    }

    bytes invert_hash (const bytes &digest) {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"invert_hash"};
    }

    key_expression::operator secp256k1::secret () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_expression::operator secp256k1::secret"};
    }

    key_expression::operator Bitcoin::secret () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_expression::operator Bitcoin::secret"};
    }

    key_expression::operator HD::BIP_32::secret () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_expression::operator HD::BIP_32::secret"};
    }

    key_expression::operator secp256k1::pubkey () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_expression::operator secp256k1::pubkey"};
    }

    key_expression::operator Bitcoin::pubkey () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_expression::operator Bitcoin::pubkey"};
    }

    key_expression::operator HD::BIP_32::pubkey () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_expression::operator HD::BIP_32::pubkey"};
    }

    key_expression key_derivation::operator () (key_expression, int32) const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_derivation::operator ()"};
    }

    key_sequence::operator std::string () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_sequence::operator string"};
    }

    key_source key_source::operator + () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_source::operator +"};
    }

    key_expression key_source::operator * () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_source::operator *"};
    }

    key_source::operator std::string () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_source::operator string"};
    }

}
