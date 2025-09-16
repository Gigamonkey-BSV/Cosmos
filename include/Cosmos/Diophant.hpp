#ifndef COSMOS_DIOPHANT
#define COSMOS_DIOPHANT

#include <Cosmos/database.hpp>
#include <Diophant/machine.hpp>

namespace Cosmos {

    void initialize (ptr<database>);
    key_expression to_private (const key_expression &);
    bytes invert_hash (const bytes &digest);

}

#endif
