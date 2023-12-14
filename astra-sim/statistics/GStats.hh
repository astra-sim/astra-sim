/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

/* DISCLAIMER:
 * This source is inspired by GStats(.h,.cpp) and ReportGen(.h,.cpp)
 * implementations in the "SESC: Super ESCalar simulator" by
 *  J. Renau, B. Fraguela, J. Tuck, W. Liu, M. Prvulovic, L. Ceze, et al.,
 *  SESC Simulator, Aug. 2005, [online] Available: http://sesc.sourceforge.net/.
 */


#pragma once

#include <fstream>
#include <list>
#include <cassert>
#include <cstdarg>
#include <cstdio>

class GStats {
 public:

  static void report();
  static GStats* getRef(const std::string& str);
  static void reset();

  GStats() {}
  virtual ~GStats();


  virtual void reportValue() const =0;
  virtual void resetValue() = 0;
  const std::string& getName() const;

  /*
   * Methods to be accessed only by the derived classes.
   */
 protected:

  std::string name;
  std::string getText(const std::string& format,va_list ap);
  void subscribe();
  void unsubscribe();
  virtual void prepareReport() {}


  /*
   * Member and methods private to GStats base class.
   */
 private:

  typedef std::list <GStats* >Container;
  typedef std::list <GStats* >::iterator ContainerIter;

  // Pointer to List of GStats* Objects.
  static Container *store;

  // Iterator to traverse the store.
  ContainerIter cpos;

};
