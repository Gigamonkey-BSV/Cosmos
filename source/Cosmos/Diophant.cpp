#include <Cosmos/Diophant.hpp>

namespace Cosmos::diophant {

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
}

namespace Cosmos {

    key_expression to_private (const key_expression &) {
        if (!bool (diophant::machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"to_private"};
    }

    bytes invert_hash (const bytes &digest) {
        if (!bool (diophant::machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"invert_hash"};
    }

    key_sequence::operator std::string () const {
        if (!bool (diophant::machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_sequence::operator string"};
    }

    key_expression key_derivation::operator () (const key_expression &k, int32 i) const {
        if (!bool (diophant::machine)) throw data::exception {} << "Diophant machine is not initialized";
        return std::string (diophant::machine->evaluate (Diophant::expression
            {string::write (static_cast<const std::string &> (*this), " $ ", static_cast<const std::string &> (k), " $ ", i)}));
    }

    key_source::operator std::string () const {
        if (!bool (diophant::machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"key_source::operator string"};
    }

}
