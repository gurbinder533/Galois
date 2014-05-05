/** Galois scheduler and runtime -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2012, The University of Texas at Austin. All rights reserved.
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
 * @section Description
 *
 * Implementation of the Galois foreach iterator. Includes various 
 * specializations to operators to reduce runtime overhead.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */
#ifndef GALOIS_RUNTIME_PARALLELWORK_H
#define GALOIS_RUNTIME_PARALLELWORK_H

#include "Galois/Mem.h"
#include "Galois/Statistic.h"
#include "Galois/Runtime/Barrier.h"
#include "Galois/Runtime/Context.h"
#include "Galois/Runtime/ForEachTraits.h"
#include "Galois/Runtime/Range.h"
#include "Galois/Runtime/Support.h"
#include "Galois/Runtime/Termination.h"
#include "Galois/Runtime/ThreadPool.h"
#include "Galois/Runtime/UserContextAccess.h"
#include "Galois/WorkList/GFifo.h"

#include <algorithm>
#include <functional>
#include <memory>

namespace Galois {
//! Internal Galois functionality - Use at your own risk.
namespace Runtime {
namespace {

template<typename value_type>
class AbortHandler {
  struct Item { value_type val; int retries; };

  typedef WorkList::GFIFO<Item> AbortedList;
  PerThreadStorage<AbortedList> queues;
  bool useBasicPolicy;
  
  /**
   * Policy: serialize via tree over packages.
   */
  void basicPolicy(const Item& item) {
    unsigned tid = LL::getTID();
    unsigned package = LL::getPackageForSelf(tid);
    queues.getRemote(LL::getLeaderForPackage(package / 2))->push(item);
  }

  /**
   * Policy: retry work 2X locally, then serialize via tree on package (trying
   * twice at each level), then serialize via tree over packages.
   */
  void doublePolicy(const Item& item) {
    int retries = item.retries - 1;
    if ((retries & 1) == 1) {
      queues.getLocal()->push(item);
      return;
    } 
    
    unsigned tid = LL::getTID();
    unsigned package = LL::getPackageForSelf(tid);
    unsigned leader = LL::getLeaderForPackage(package);
    if (tid != leader) {
      unsigned next = leader + (tid - leader) / 2;
      queues.getRemote(next)->push(item);
    } else {
      queues.getRemote(LL::getLeaderForPackage(package / 2))->push(item);
    }
  }

  /**
   * Policy: retry work 2X locally, then serialize via tree on package but
   * try at most 3 levels, then serialize via tree over packages.
   */
  void boundedPolicy(const Item& item) {
    int retries = item.retries - 1;
    if (retries < 2) {
      queues.getLocal()->push(item);
      return;
    } 
    
    unsigned tid = LL::getTID();
    unsigned package = LL::getPackageForSelf(tid);
    unsigned leader = LL::getLeaderForPackage(package);
    if (retries < 5 && tid != leader) {
      unsigned next = leader + (tid - leader) / 2;
      queues.getRemote(next)->push(item);
    } else {
      queues.getRemote(LL::getLeaderForPackage(package / 2))->push(item);
    }
  }

  /**
   * Retry locally only.
   */
  void eagerPolicy(const Item& item) {
    queues.getLocal()->push(item);
  }

public:
  AbortHandler() {
    // XXX(ddn): Implement smarter adaptive policy
    useBasicPolicy = LL::getMaxPackages() > 2;
  }

  value_type& value(Item& item) const { return item.val; }
  value_type& value(value_type& val) const { return val; }

  void push(const value_type& val) {
    Item item = { val, 1 };
    queues.getLocal()->push(item);
  }

  void push(const Item& item) {
    Item newitem = { item.val, item.retries + 1 };
    if (useBasicPolicy)
      basicPolicy(newitem);
    else
      doublePolicy(newitem);
  }

  AbortedList* getQueue() { return queues.getLocal(); }
};

template<class WorkListTy, class T, class FunctionTy>
class ForEachWork {
protected:
  typedef T value_type;

  struct ThreadLocalData {
    FunctionTy function;
    UserContextAccess<value_type> facing;
    SimpleRuntimeContext ctx;
    unsigned long stat_conflicts;
    unsigned long stat_commits;
    unsigned long stat_pushs;
    const char* loopname;
    ThreadLocalData(const FunctionTy& fn, const char* ln)
      : function(fn), stat_conflicts(0), stat_commits(0), stat_pushs(0), 
        loopname(ln)
    {}
    ~ThreadLocalData() {
      if (ForEachTraits<FunctionTy>::NeedsStats) {
        reportStat(loopname, "Conflicts", stat_conflicts);
        reportStat(loopname, "Commits", stat_commits);
        reportStat(loopname, "Pushs", stat_pushs);
        reportStat(loopname, "Iterations", stat_commits + stat_conflicts);
      }
    }
  };

  // NB: Place dynamically growing wl after fixed-size PerThreadStorage
  // members to give higher likelihood of reclaiming PerThreadStorage

  AbortHandler<value_type> aborted; 
  TerminationDetection& term;

  WorkListTy wl;
  FunctionTy& origFunction;
  const char* loopname;
  bool broke;

  inline void commitIteration(ThreadLocalData& tld) {
    if (ForEachTraits<FunctionTy>::NeedsPush) {
      auto ii = tld.facing.getPushBuffer().begin();
      auto ee = tld.facing.getPushBuffer().end();
      auto& pb = tld.facing.getPushBuffer();
      auto n = pb.size();
      if (n) {
	tld.stat_pushs += n;
        wl.push(pb.begin(), pb.end());
        pb.clear();
      }
    }
    if (ForEachTraits<FunctionTy>::NeedsPIA)
      tld.facing.resetAlloc();
    if (ForEachTraits<FunctionTy>::NeedsAborts)
      tld.ctx.commitIteration();
    ++tld.stat_commits;
  }

  template<typename Item>
  GALOIS_ATTRIBUTE_NOINLINE
  void abortIteration(const Item& item, ThreadLocalData& tld) {
    assert(ForEachTraits<FunctionTy>::NeedsAborts);
    tld.ctx.cancelIteration();
    ++tld.stat_conflicts;
    aborted.push(item);
    //clear push buffer
    if (ForEachTraits<FunctionTy>::NeedsPush)
      tld.facing.resetPushBuffer();
    //reset allocator
    if (ForEachTraits<FunctionTy>::NeedsPIA)
      tld.facing.resetAlloc();
  }

  inline void doProcess(value_type& val, ThreadLocalData& tld) {
    if (ForEachTraits<FunctionTy>::NeedsAborts)
      tld.ctx.startIteration();
    tld.function(val, tld.facing.data());
    commitIteration(tld);
  }

  void runQueueSimple(ThreadLocalData& tld) {
    Galois::optional<value_type> p;
    while ((p = wl.pop())) {
      doProcess(*p, tld);
    }
  }

  template<int limit, typename WL>
  void runQueue(ThreadLocalData& tld, WL& lwl) {
    Galois::optional<typename WL::value_type> p;
    unsigned num = 0;
    try {
      while ((!limit || num++ < limit) && (p = lwl.pop())) {
	doProcess(aborted.value(*p), tld);
      }
    } catch (ConflictFlag const& flag) {
      abortIteration(*p, tld);
    }
  }

  GALOIS_ATTRIBUTE_NOINLINE
  void handleAborts(ThreadLocalData& tld) {
    runQueue<0>(tld, *aborted.getQueue());
  }

  void fastPushBack(typename UserContextAccess<value_type>::PushBufferTy& x) {
    wl.push(x.begin(), x.end());
    x.clear();
  }

  template<bool couldAbort, bool isLeader>
  void go() {
    // Thread-local data goes on the local stack to be NUMA friendly
    ThreadLocalData tld(origFunction, loopname);
    if (ForEachTraits<FunctionTy>::NeedsBreak)
      tld.facing.setBreakFlag(&broke);
    if (couldAbort)
      setThreadContext(&tld.ctx);
    if (ForEachTraits<FunctionTy>::NeedsPush && !couldAbort)
      tld.facing.setFastPushBack(
          std::bind(&ForEachWork::fastPushBack, this, std::placeholders::_1));
    unsigned old_commit = 0;
    do {
      // Run some iterations
      if (couldAbort || ForEachTraits<FunctionTy>::NeedsBreak) {
        constexpr int __NUM = (ForEachTraits<FunctionTy>::NeedsBreak || isLeader) ? 32 : 0;
        runQueue<__NUM>(tld, wl);
        // Check for abort
        if (couldAbort)
          handleAborts(tld);
      } else { // No try/catch
        runQueueSimple(tld);
      }
      // Update node color and prop token
      term.localTermination(old_commit != tld.stat_commits);
      old_commit = tld.stat_commits;
      LL::asmPause(); // Let token propagate
    } while (!term.globalTermination() && (!ForEachTraits<FunctionTy>::NeedsBreak || !broke));

    if (couldAbort)
      setThreadContext(0);
  }

public:
  ForEachWork(FunctionTy& f, const char* l): term(getSystemTermination()), origFunction(f), loopname(l), broke(false) { }
  
  template<typename W>
  ForEachWork(W& w, FunctionTy& f, const char* l): term(getSystemTermination()), wl(w), origFunction(f), loopname(l), broke(false) { }

  template<typename RangeTy>
  void AddInitialWork(const RangeTy& range) {
    wl.push_initial(range);
  }

  void initThread(void) {
    term.initializeThread();
  }

  void operator()() {
    bool isLeader = LL::isPackageLeaderForSelf(LL::getTID());
    bool couldAbort = ForEachTraits<FunctionTy>::NeedsAborts && activeThreads > 1;
    if (couldAbort && isLeader)
      go<true, true>();
    else if (couldAbort && !isLeader)
      go<true, false>();
    else if (!couldAbort && isLeader)
      go<false, true>();
    else
      go<false, false>();
  }
};

template<typename WLTy, typename RangeTy, typename FunctionTy>
void for_each_impl(const RangeTy& range, FunctionTy f, const char* loopname) {
  if (inGaloisForEach)
    GALOIS_DIE("Nested for_each not supported");

  typedef typename RangeTy::value_type T;
  typedef ForEachWork<typename WLTy::template retype<T>::type, T, FunctionTy> WorkTy;

  // NB: Initialize barrier before creating WorkTy to increase
  // PerThreadStorage reclaimation likelihood
  Barrier& barrier = getSystemBarrier();

  WorkTy W(f, loopname);

  // StatTimer LoopTimer("LoopTime", loopname);
  // if (ForEachTraits<FunctionTy>::NeedsStats)
  //   LoopTimer.start();

  inGaloisForEach = true;
  getSystemThreadPool().run(activeThreads,
                            [&W] () {W.initThread();},
                            [&W, &range] {W.AddInitialWork(range);},
                            std::ref(barrier),
                            std::ref(W));
  inGaloisForEach = false;

  // if (ForEachTraits<FunctionTy>::NeedsStats)  
  //   LoopTimer.stop();
}

template<typename FunctionTy>
struct WOnEach {
  FunctionTy& origFunction;
  WOnEach(FunctionTy& f): origFunction(f) { }
  void operator()(void) {
    FunctionTy fn(origFunction);
    fn(LL::getTID(), activeThreads);   
  }
};

template<typename FunctionTy>
void on_each_impl(FunctionTy fn, const char* loopname = 0) {
  if (inGaloisForEach)
    GALOIS_DIE("Nested for_each not supported");

  inGaloisForEach = true;
  getSystemThreadPool().run(activeThreads, WOnEach<FunctionTy>(fn));
  inGaloisForEach = false;
}

} // end namespace anonymous

void preAlloc_impl(int num);

} // end namespace Runtime
} // end namespace Galois

#endif

