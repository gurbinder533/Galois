#include "Galois/Runtime/sync_structures.h"

////////////////////////////////////////////////////////////////////////////
// ToAdd
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_ADD(to_add, unsigned int);
GALOIS_SYNC_STRUCTURE_REDUCE_SET(to_add, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(to_add, unsigned int);
GALOIS_SYNC_STRUCTURE_BITSET(to_add);

////////////////////////////////////////////////////////////////////////////
// ToAddFloat
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_ADD(to_add_float, float);
GALOIS_SYNC_STRUCTURE_REDUCE_SET(to_add_float, float);
GALOIS_SYNC_STRUCTURE_BROADCAST(to_add_float, float);
GALOIS_SYNC_STRUCTURE_BITSET(to_add_float);

////////////////////////////////////////////////////////////////////////////
// # short paths
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_SET(num_shortest_paths, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(num_shortest_paths, unsigned int);
GALOIS_SYNC_STRUCTURE_BITSET(num_shortest_paths);

////////////////////////////////////////////////////////////////////////////
// Succ
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_ADD(num_successors, unsigned int);
GALOIS_SYNC_STRUCTURE_REDUCE_SET(num_successors, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(num_successors, unsigned int);
GALOIS_SYNC_STRUCTURE_BITSET(num_successors);

////////////////////////////////////////////////////////////////////////////
// Pred
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_ADD(num_predecessors, unsigned int);
GALOIS_SYNC_STRUCTURE_REDUCE_SET(num_predecessors, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(num_predecessors, unsigned int);
GALOIS_SYNC_STRUCTURE_BITSET(num_predecessors);

////////////////////////////////////////////////////////////////////////////
// Trim
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_ADD(trim, unsigned int);
GALOIS_SYNC_STRUCTURE_REDUCE_SET(trim, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(trim, unsigned int);
GALOIS_SYNC_STRUCTURE_BITSET(trim);

////////////////////////////////////////////////////////////////////////////
// Current Lengths
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_MIN(current_length, unsigned int);
GALOIS_SYNC_STRUCTURE_REDUCE_SET(current_length, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(current_length, unsigned int);
GALOIS_SYNC_STRUCTURE_BITSET(current_length);

////////////////////////////////////////////////////////////////////////////////
// Old length
////////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_SET(old_length, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(old_length, unsigned int);

////////////////////////////////////////////////////////////////////////////
// Flag
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_SET(propogation_flag, uint8_t);
GALOIS_SYNC_STRUCTURE_BROADCAST(propogation_flag, uint8_t);
GALOIS_SYNC_STRUCTURE_BITSET(propogation_flag);

////////////////////////////////////////////////////////////////////////////
// Dependency
////////////////////////////////////////////////////////////////////////////

GALOIS_SYNC_STRUCTURE_REDUCE_SET(dependency, float);
GALOIS_SYNC_STRUCTURE_BROADCAST(dependency, float);
GALOIS_SYNC_STRUCTURE_BITSET(dependency);