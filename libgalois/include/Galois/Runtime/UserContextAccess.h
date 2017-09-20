/** Manipulate the user context -*- C++ -*-
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
 * @author Andrew Lenharth <andrew@lenharth.org>
 */

#ifndef GALOIS_RUNTIME_USERCONTEXTACCESS_H
#define GALOIS_RUNTIME_USERCONTEXTACCESS_H

#include "Galois/UserContext.h"

namespace galois {
namespace Runtime {

//! Backdoor to allow runtime methods to access private data in UserContext
template<typename T>
class UserContextAccess : public galois::UserContext<T> {
public:
  typedef galois::UserContext<T> SuperTy;
  typedef typename SuperTy::PushBufferTy PushBufferTy;
  typedef typename SuperTy::FastPushBack FastPushBack;

  void resetAlloc() { SuperTy::__resetAlloc(); }
  PushBufferTy& getPushBuffer() { return SuperTy::__getPushBuffer(); }
  void resetPushBuffer() { SuperTy::__resetPushBuffer(); }
  SuperTy& data() { return *static_cast<SuperTy*>(this); }
  void setLocalState(void *p) { SuperTy::__setLocalState(p); }
  void setFastPushBack(FastPushBack f) { SuperTy::__setFastPushBack(f); }
  void setBreakFlag(bool *b) { SuperTy::didBreak = b; }

  void setFirstPass (void) { SuperTy::__setFirstPass(); }
  void resetFirstPass (void) { SuperTy::__resetFirstPass(); }

// TODO: move to a separate class dedicated for speculative executors
#ifdef GALOIS_USE_EXP
  void rollback() { SuperTy::__rollback (); }

  void commit() { SuperTy::__commit (); }

  void reset() {
    SuperTy::__resetPushBuffer();
    SuperTy::__resetUndoLog();
    SuperTy::__resetCommitLog();
    SuperTy::__resetAlloc();
  }
#endif
};

}
} // end namespace galois

#endif