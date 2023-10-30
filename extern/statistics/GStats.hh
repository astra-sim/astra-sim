//
// Created by dkadiyala3 on 10/29/23.
//

#ifndef ASTRASIM_ANALYTICAL_GSTATS_HH
#define ASTRASIM_ANALYTICAL_GSTATS_HH
#include <fstream>
#include <list>
#include <cassert>
#include <cstdarg>
#include <cstdio>

class GStats {
 public:

  static void report(void);
  static GStats* getRef(const std::string& str);
  static void reset(void);

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
  void subscribe(void);
  void unsubscribe(void);
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
#endif // ASTRASIM_ANALYTICAL_GSTATS_HH
