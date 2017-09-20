#include "PageRankDet.h"


template <typename B>
struct NodeData: public B, PData {

  NodeData (void)
    : B (0), PData ()
  {}

  NodeData (unsigned id, unsigned outdegree)
    : B (id), PData (outdegree)
  {}


};

template <galois::Runtime::InputDAG_ExecTy EXEC>
struct ChooseNodeDataType {
  using type = galois::Runtime::InputDAGdataInOut;
};

template <>
struct ChooseNodeDataType<galois::Runtime::InputDAG_ExecTy::PART> { 
  using type = galois::Runtime::InputDAGdataPartInOut;
};




template <galois::Runtime::InputDAG_ExecTy EXEC>
struct ChooseInnerGraph {
  using ND = NodeData<typename ChooseNodeDataType<EXEC>::type>; 

  using type = typename galois::Graph::LC_CSR_Graph<ND, void>
    ::template with_numa_alloc<true>::type
    ::template with_no_lockable<true>::type;
};

template <galois::Runtime::InputDAG_ExecTy EXEC>
class PageRankInputDAG: public PageRankBase<typename ChooseInnerGraph<EXEC>::type> {
protected:

  using Base = PageRankBase<typename ChooseInnerGraph<EXEC>::type>;
  using GNode = typename Base::GNode;

  struct ApplyOperator {

    static const unsigned CHUNK_SIZE = Base::DEFAULT_CHUNK_SIZE;
    static const unsigned UNROLL_FACTOR = 32;

    PageRankInputDAG& outer;

    template <typename C>
    void operator () (GNode src, C& ctx) {
      outer.applyOperator (src, ctx);
    }
  };

  virtual void runPageRank (void) {
    galois::Runtime::ForEachDet_InputDAG<EXEC>::run (
        galois::Runtime::makeLocalRange(Base::graph),
        ApplyOperator {*this},
        Base::graph,
        "page-rank-input-dag"
        );
  }

};

int main (int argc, char* argv[]) {
  LonestarStart (argc, argv, name, desc, url);

  switch (galois::Runtime::inputDAG_ExecTy) {
    case galois::Runtime::InputDAG_ExecTy::CHROMATIC: 
      {
        PageRankInputDAG<galois::Runtime::InputDAG_ExecTy::CHROMATIC> p;
        p.run ();
        break;
      }
    case galois::Runtime::InputDAG_ExecTy::EDGE_FLIP: 
      {
        PageRankInputDAG<galois::Runtime::InputDAG_ExecTy::EDGE_FLIP> p;
        p.run ();
        break;
      }
    case galois::Runtime::InputDAG_ExecTy::TOPO: 
      {
        PageRankInputDAG<galois::Runtime::InputDAG_ExecTy::TOPO> p;
        p.run ();
        break;
      }
    case galois::Runtime::InputDAG_ExecTy::PART: 
      {
        PageRankInputDAG<galois::Runtime::InputDAG_ExecTy::PART> p;
        p.run ();
        break;
      }
    case galois::Runtime::InputDAG_ExecTy::HYBRID: 
      {
        PageRankInputDAG<galois::Runtime::InputDAG_ExecTy::HYBRID> p;
        p.run ();
        break;
      }

    default:
      std::abort ();
  }

} // end main

