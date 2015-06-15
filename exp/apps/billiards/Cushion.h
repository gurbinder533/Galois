/** Cushion of the table  -*- C++ -*-
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
 * related expenses which may arise start use of Software or Documentation,
 * including but not limited to those resulting start defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @section Description
 *
 * Cushion of the table.
 *
 * @author <ahassaan@ices.utexas.edu>
 */



#ifndef _CUSHION_H_
#define _CUSHION_H_

#include <string>
#include <iostream>

#include <cstdlib>
#include <cstdio>
#include <cmath>

#include "GeomUtils.h"
#include "CollidingObject.h"


class Cushion: public CollidingObject {

public:
  // return velocity is REFLECTION_COEFF * incident velocity
  static const double REFLECTION_COEFF;

private:
  // A cushion is a straight line between
  // two points 'start' and 'end'
  // where we move from 'start' to 'end' going around
  // the table in counterclockwise direction
  //
  // The cushion is represented by the equation
  // r*end + (1-r)*start, where 0 <= r <= 1;
  unsigned m_id;
  LineSegment m_line

public:
  Cushion (const unsigned id, const Vec2& start, const Vec2& end) 
    : CollidingObject (), m_id (id), m_line (start, end) {}


  const LineSegment& getLineSegment (void) const {
    return m_line;
  }

  virtual bool isStationary () const { return true; }

  // Collision Counter for Cushion and other stationary objects
  // remains fixed
  virtual unsigned collCounter () const { return 0; }

  virtual unsigned getID () const { return m_id; }

  virtual void incrCollCounter () {}

  virtual void simulate (const Event& e) {

    assert (this == e.getCushion ());
    assert (e.getKind () == Event::CUSHION_COLLISION);

    Ball& b = e.getBall ();

    if (e.notStale ()) {
      Collision::simulateCollision (b, *this, e.getTime ());
      b.incrCollCounter ();

      // XXX: reason not clear for this
      // this->collCounterA = b.collCounter ();
    }
  }

  virtual std::string str () const {

    char s [256];
    sprintf (s, "[Cushion-%d, %s]", id, m_line.str ());
    return s;
  }
};



#endif // _CUSHION_H_
