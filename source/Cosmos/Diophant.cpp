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

    key_expression key_expression::to_public () const {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        key_expression result {std::string (machine->evaluate (Diophant::expression {data::string::write ("to_public (", *this, ")")}))};
        if (!result.valid ()) return {};
        return result;
    }

    key_expression to_private (const key_expression &) {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"to_private"};
    }

    bytes invert_hash (const bytes &digest) {
        if (!bool (machine)) throw data::exception {} << "Diophant machine is not initialized";
        throw data::method::unimplemented {"invert_hash"};
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
