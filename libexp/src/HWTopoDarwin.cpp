/** Machine Descriptions on Darwin -*- C++ -*-
 * @file
 * @section License
 *
 * This file is part of Galois.  Galoisis a gramework to exploit
 * amorphous data-parallelism in irregular programs.
 *
 * Galois is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 2.1 of the
 * License.
 *
 * Galois is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Galois.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * @section Copyright
 *
 * Copyright (C) 2015, The University of Texas at Austin. All rights
 * reserved.
 *
 * @section Description
 *
 * See HWTopoLinux.cpp.
 * 
 * @author Donald Nguyen <ddn@cs.utexas.edu>
 */
#include "Galois/Runtime/ll/HWTopo.h"
#include "Galois/Runtime/ll/gio.h"
#include <sys/types.h>
#include <sys/sysctl.h>

using namespace Galois::Runtime::LL;

namespace {

struct Policy {
  //number of "real" processors
  uint32_t numCpus;

  Policy() {
    size_t bufSize = sizeof(numCpus);
    if (sysctlbyname("hw.activecpu", &numCpus, &bufSize, NULL, 0) == -1) {
      GALOIS_SYS_DIE("Error querying number of cpus");
    } else if (bufSize != sizeof(numCpus)) {
      GALOIS_SYS_DIE("Error querying number of cpus");
    }
  }
};

static Policy& getPolicy() {
  static Policy A;
  return A;
}

} //namespace

bool Galois::Runtime::LL::bindThreadToProcessor(int id) {
  return false;
}

unsigned Galois::Runtime::LL::getProcessorForThread(int id) {
  return id;
}

unsigned Galois::Runtime::LL::getMaxThreads() {
  return getPolicy().numCpus;
}

unsigned Galois::Runtime::LL::getMaxCores() {
  return getPolicy().numCpus;
}

unsigned Galois::Runtime::LL::getMaxPackages() {
  return getPolicy().numCpus;
}

unsigned Galois::Runtime::LL::getPackageForThread(int id) {
  return id;
}

unsigned Galois::Runtime::LL::getMaxPackageForThread(int id) {
  return id;
}

bool Galois::Runtime::LL::isPackageLeader(int id) {
  return true;
}

unsigned Galois::Runtime::LL::getLeaderForThread(int id) {
  return id;
}

unsigned Galois::Runtime::LL::getLeaderForPackage(int id) {
  return id;
}
