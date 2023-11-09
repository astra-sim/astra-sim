//
// Created by dkadiyala3 on 10/29/23.
//

#include "GStats.hh"
#include <cassert>
#include <cstdarg>
#include <cstdio>

// Initialize the static pointer to GStats::store.
GStats::Container* GStats::store=0;

// Define Constructor and Destructor here
GStats::~GStats()
{
  for(ContainerIter i = store->begin(); i != store->end(); i++) {
    if(*i == this) {
      store->erase(i);
    }
  }
}

void GStats::report(void)
{
  //Report::flush();
  //Report::field("BEGIN GStats::report %s", str);

  if (store) {
    for(ContainerIter i = store->begin(); i != store->end(); i++) {
      (*i)->prepareReport(); //give class a chance to do any calculations
      (*i)->reportValue();
    }
  }

  //Report::field("END GStats::report %s", str);
  //Report::flush();
}


void GStats::subscribe(void) {
  if( store == 0 ) {
    store = new Container;
  }
  // Get the iterator the current position in the list.
  cpos=store->emplace(store->end(),this);
  // Later add a message mentioning a container has been sunscribed.
}

void GStats::unsubscribe(void)
{
  assert(store);
  assert(*cpos==this);
  //store->erase(cpos);
  bool found = false;
  //I(store);
   for(ContainerIter it = store->begin(); it != store->end(); ++it) {
     if(*it == this) {
       store->erase(it);
       break;
     }
   }
   // Later: add an info message that a particular container has been unsubscribed
}


void GStats::reset() {
  if (store) {
    for(ContainerIter it = store->begin(); it != store->end(); it++) {
      (*it)->resetValue();
    }
  }
}

GStats* GStats::getRef(const std::string& str)
{
  for(ContainerIter it = store->begin(); it != store->end(); it++) {

    if((*it)->name == str)
      return *it;
  }
  return nullptr;
}

const std::string& GStats::getName() const {
  return name;
}


std::string GStats::getText(const std::string& format, va_list ap)
{

  char buffer[1024];
  vsprintf(buffer, format.c_str(), ap);
  return (buffer);
}
