/** GMetis -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2011, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @author Xin Sui <xinsui@cs.utexas.edu>
 * @author Nikunj Yadav <nikunj@cs.utexas.edu>
 * @author Andrew Lenharth <andrew@lenharth.org>
 */

#include "GMetisConfig.h"
#include "MetisGraph.h"
#include "Metis.h"

namespace {

void assertAllMatched(GNode node, GGraph* graph) {
  for (auto jj = graph->edge_begin(node), eejj = graph->edge_end(node);
       jj != eejj; ++jj)
    assert(node == graph->getEdgeDst(jj) || graph->getData(graph->getEdgeDst(jj)).isMatched());
}

void assertNoMatched(GGraph* graph) {
  for (auto nn = graph->begin(), en = graph->end(); nn != en; ++nn)
    assert(!graph->getData(*nn).isMatched());
}

struct HEMmatch {
  // unsigned matched;
  // unsigned self;
  // unsigned checked;
  // unsigned nume;
  ~HEMmatch() {
    //    std::cout << Galois::Runtime::LL::getTID() << " matched " << matched << " self " << self << " checked " << checked << " edges of " << nume << "\n";
  }
  GNode operator()(GNode node, GGraph* graph) {
    GNode retval = node; // match self if nothing else
    int maxwgt = std::numeric_limits<int>::min();
    //    nume += std::distance(graph->edge_begin(node), graph->edge_end(node));
    for (auto jj = graph->edge_begin(node, Galois::MethodFlag::NONE), eejj = graph->edge_end(node);
         jj != eejj; ++jj) {
      //      ++checked;
      GNode neighbor = graph->getEdgeDst(jj, Galois::MethodFlag::NONE);
      MetisNode& neighMNode = graph->getData(neighbor, Galois::MethodFlag::NONE);
      int edgeData = graph->getEdgeData(jj, Galois::MethodFlag::NONE);
      if (!neighMNode.isMatched() && neighbor != node && maxwgt < edgeData) {
        maxwgt = edgeData;
        retval = neighbor;
      }
    }
    // assert(!graph->getData(retval).isMatched());
    // assert(!graph->getData(node).isMatched());
    // if (retval == node)
    //   ++self;
    // else
    //   ++matched;
    return retval;
  }
};

struct RMmatch {
  GNode operator()(GNode node, GGraph* graph) {
    for (auto jj = graph->edge_begin(node, Galois::MethodFlag::NONE), eejj = graph->edge_end(node);
         jj != eejj; ++jj) {
      GNode neighbor = graph->getEdgeDst(jj, Galois::MethodFlag::NONE);
      if (!graph->getData(neighbor, Galois::MethodFlag::NONE).isMatched() && neighbor != node)
        return neighbor;
    }
    return node;
    //Don't actually do random, just choose first
  }
};

/*
 *This operator is responsible for matching.
 1. There are two types of matching. Random and Heavy Edge matching 
 2. Random matching picks any random node above a threshold and matches the nodes. RM.h 
 3. Heavy Edge Matching matches the vertex which is connected by the heaviest edge. HEM.h 
 4. This operator can also create the multinode, i.e. the node which is created on combining two matched nodes.  
 5. You can enable/disable 4th by changing variantMetis::mergeMatching
*/
template<typename MatchingPolicy>
struct parallelMatchAndCreateNodes {
  //typedef int tt_does_not_need_parallel_push;
  MatchingPolicy matcher;
  GGraph *fineGGraph;
  GGraph *coarseGGraph;
  unsigned dual;
  parallelMatchAndCreateNodes(MetisGraph* Graph)
    : matcher(), fineGGraph(Graph->getFinerGraph()->getGraph()), coarseGGraph(Graph->getGraph()), dual(0) {
    assert(fineGGraph != coarseGGraph);
  }

  ~parallelMatchAndCreateNodes() { if (dual) std::cout << "Dual matched " << dual << "\n"; }

  void operator()(GNode item, Galois::UserContext<GNode> &lwl) {
    if (fineGGraph->getData(item).isMatched())
      return;
    GNode ret;
    do {
      ret = matcher(item, fineGGraph);
      //lock ret, since we found it lock-free it may be matched, so try again
    } while (fineGGraph->getData(ret).isMatched());

    //at this point both ret and item (and failed matches) are locked.
    //We do not leave the above loop until we both have the lock on
    //the node and check the matched status of the locked node.  the
    //lock before (final) read ensures that we will see any write to matched

    unsigned numEdges = fineGGraph->getData(item).getNumEdges();
    //assert(numEdges == std::distance(fineGGraph->edge_begin(item), fineGGraph->edge_end(item)));

    GNode N;
    if (ret != item) {
      //__sync_fetch_and_add(&dual, 1);
      //match found
      numEdges += fineGGraph->getData(ret).getNumEdges();
      //Cautious point
      N = coarseGGraph->createNode(numEdges, item, ret, 
                                   fineGGraph->getData(item).getWeight() +
                                   fineGGraph->getData(ret).getWeight() );
    } else {
      //assertAllMatched(item, fineGGraph);
      //Cautious point
      //no match
      N = coarseGGraph->createNode(numEdges, item, fineGGraph->getData(item).getWeight());
    }
    fineGGraph->getData(item).setMatched(ret);
    fineGGraph->getData(ret).setMatched(item);
    fineGGraph->getData(item).setParent(N);
    fineGGraph->getData(ret).setParent(N);
  }
};

/*
 * This operator is responsible for doing a union find of the edges
 * between matched nodes and populate the edges in the coarser graph
 * node.
 */

struct parallelPopulateEdges {
  typedef int tt_does_not_need_parallel_push;
  typedef int tt_needs_per_iter_alloc;
    
  GGraph *coarseGGraph;
  GGraph *fineGGraph;
  parallelPopulateEdges(MetisGraph *Graph)
    :coarseGGraph(Graph->getGraph()), fineGGraph(Graph->getFinerGraph()->getGraph()) {
    assert(fineGGraph != coarseGGraph);
  }

  template<typename Context>
  void operator()(GNode node, Context& lwl) {
    //    std::cout << 'p';
    //fineGGraph is read only in this loop, so skip locks
    MetisNode &nodeData = coarseGGraph->getData(node);

    unsigned e = 0;
    typedef Galois::flat_map<GNode, unsigned, std::less<GNode>, Galois::PerIterAllocTy::rebind<std::pair<GNode,unsigned> >::other> GD;
    //copy and translate all edges
    GD edges(std::less<GNode>(), GD::allocator_type(lwl.getPerIterAlloc()));
    for (unsigned x = 0; x < nodeData.numChildren(); ++x)
      for (auto ii = fineGGraph->edge_begin(nodeData.getChild(x), Galois::MethodFlag::NONE), ee = fineGGraph->edge_end(nodeData.getChild(x)); ii != ee; ++ii)
        edges[fineGGraph->getData(fineGGraph->getEdgeDst(ii, Galois::MethodFlag::NONE), Galois::MethodFlag::NONE).getParent()] += fineGGraph->getEdgeData(ii, Galois::MethodFlag::NONE);
    
    //insert edges
    for (auto pp = edges.begin(), ep = edges.end(); pp != ep; ++pp) {
      if (node != pp->first) { // no self edges
        coarseGGraph->getEdgeData(coarseGGraph->addEdgeWithoutCheck(node, pp->first), Galois::MethodFlag::NONE) = pp->second;
        ++e;
      }
    }
    //    assert(e);
    nodeData.setNumEdges(e);
  }
};

struct DegreeIndexer: public std::unary_function<GNode, unsigned int> {
  static GGraph* indexgraph;
  unsigned int operator()(const GNode& val) const {
    return std::numeric_limits<unsigned int>::max() - indexgraph->getData(val, Galois::MethodFlag::NONE).getNumEdges();
  }
};
GGraph* DegreeIndexer::indexgraph = 0;

struct WeightIndexer: public std::unary_function<GNode, int> {
  int operator()(const GNode& val) const {
    return DegreeIndexer::indexgraph->getData(val, Galois::MethodFlag::NONE).getWeight();
  }
};

unsigned minRuns(unsigned coarsenTo, unsigned size) {
  unsigned num = 0;
  while (coarsenTo < size) {
    ++num;
    size /= 2;
  }
  return num;
}

void findMatching(MetisGraph* coarseMetisGraph, unsigned iterNum, bool useRM = false) {
  MetisGraph* fineMetisGraph = coarseMetisGraph->getFinerGraph();

  /*
   * Different worklist versions tried, dChunkedFIFO 256 works best with LC_MORPH_graph.
   * Another good type would be Lazy Iter.
   */
  //typedef Galois::WorkList::ChunkedLIFO<64, GNode> WL;
  //typedef Galois::WorkList::LazyIter<decltype(fineGGraph->local_begin()),false> WL;

  if(useRM) {
    typedef decltype(fineMetisGraph->getGraph()->local_begin()) ITY;
    typedef Galois::WorkList::StableIterator<ITY, true> WL;
    parallelMatchAndCreateNodes<RMmatch> pRM(coarseMetisGraph);
    std::ostringstream name;
    name << "RM_Match_" << iterNum;
    Galois::for_each_local<WL>(*fineMetisGraph->getGraph(), pRM, name.str().c_str());
  } else {
    //FIXME: use obim for SHEM matching
    typedef decltype(fineMetisGraph->getGraph()->local_begin()) ITY;
    typedef Galois::WorkList::StableIterator<ITY, true> WL;
    typedef Galois::WorkList::dChunkedFIFO<8> Chunk;
    typedef Galois::WorkList::OrderedByIntegerMetric<WeightIndexer, Chunk, 10> OBIM;
    DegreeIndexer::indexgraph = fineMetisGraph->getGraph();
    parallelMatchAndCreateNodes<HEMmatch> pHEM(coarseMetisGraph);
    std::ostringstream name;
    name << "HEM_Match_" << iterNum;
    Galois::for_each_local<OBIM>(*fineMetisGraph->getGraph(), pHEM, name.str().c_str());
  }
}

void createCoarseEdges(MetisGraph *coarseMetisGraph, unsigned iterNum) {
  MetisGraph* fineMetisGraph = coarseMetisGraph->getFinerGraph();
  GGraph* fineGGraph = fineMetisGraph->getGraph();
  typedef Galois::WorkList::StableIterator<decltype(fineGGraph->local_begin()), true> WL;
  parallelPopulateEdges pPE(coarseMetisGraph);
  std::ostringstream name;
  name << "Populate_Edges_" << iterNum;
  Galois::for_each_local<WL>(*coarseMetisGraph->getGraph(), pPE, name.str().c_str());
}

MetisGraph* coarsenOnce(MetisGraph *fineMetisGraph, unsigned iterNum, bool useRM = false) {
  MetisGraph *coarseMetisGraph = new MetisGraph(fineMetisGraph);
  //assertNoMatched(fineMetisGraph->getGraph());
  // Galois::Timer t;
  // t.start();
  findMatching(coarseMetisGraph, iterNum, useRM);
  // t.stop();
  // std::cout << "Time Matching " << iterNum << " is " << t.get() << "\n";
  //assertNoMatched(coarseMetisGraph->getGraph());
  // Galois::Timer t2;
  // t2.start();
  createCoarseEdges(coarseMetisGraph, iterNum);
  // t2.stop();
  // std::cout << "Time Creating " << iterNum << " is " << t2.get() << "\n";
  return coarseMetisGraph;
}

} // anon namespace

void graphStat(GGraph* graph) {
  unsigned numEdges = 0;
  std::map<unsigned, unsigned> hist;
  for (auto ii = graph->begin(), ee = graph->end(); ii != ee; ++ii) {
    unsigned val = std::distance(graph->edge_begin(*ii), graph->edge_end(*ii));
    numEdges += val;
    ++hist[val];
  }
  
  std::cout<<"Nodes "<<std::distance(graph->begin(), graph->end())<<"| Edges " << numEdges << endl;
  for (auto pp = hist.begin(), ep = hist.end(); pp != ep; ++pp)
    std::cout << pp->first << " : " << pp->second << "\n";
}

MetisGraph* coarsen(MetisGraph* fineMetisGraph, unsigned coarsenTo) {
  unsigned iRuns = minRuns(coarsenTo, std::distance(fineMetisGraph->getGraph()->begin(), fineMetisGraph->getGraph()->end()));
  
  //std::cout << "coarsening stating, now " <<  std::distance(fineMetisGraph->getGraph()->begin(), fineMetisGraph->getGraph()->end()) << " on " << fineMetisGraph << "\n";
  //graphStat(fineMetisGraph->getGraph());

  MetisGraph* coarseGraph = fineMetisGraph;

  unsigned iterNum = 0;
  while (iRuns) {
    coarseGraph = coarsenOnce(coarseGraph, iterNum, iterNum == 0);
    //std::cout << "coarsening " << iterNum << " done, now " <<  std::distance(coarseGraph->getGraph()->begin(), coarseGraph->getGraph()->end()) << " on " << coarseGraph << " targeting " << coarsenTo << "\n";
    //    graphStat(coarseGraph->getGraph());
    if (iRuns)
      --iRuns;
    if (!iRuns)
      iRuns = minRuns(coarsenTo, std::distance(coarseGraph->getGraph()->begin(), coarseGraph->getGraph()->end()));
    ++iterNum;
  }
  return coarseGraph;
}