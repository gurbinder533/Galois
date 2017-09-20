#include "GaloisDag.hpp"

void galoisAllocation(Node *node, SolverMode mode)
{
    galois::Runtime::for_each_ordered_tree(node, GaloisAllocationDivide(),
        GaloisAllocationConquer(mode), "GaloisAllocation");
}

void galoisElimination (Node *node)
{
    galois::Runtime::for_each_ordered_tree(node, GaloisEliminationDivide(),
        GaloisEliminationConquer(), "GaloisElimination");
}

void galoisBackwardSubstitution(Node *node)
{
    galois::Runtime::for_each_ordered_tree(node, GaloisBackwardSubstitutionDivide(),
        GaloisBackwardSubstitutionConquer(), "GaloisBackwardSubstitution");
}