////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef __ARANGODB_CONSENSUS_STORE__
#define __ARANGODB_CONSENSUS_STORE__

#include "AgencyCommon.h"

#include <type_traits>
#include <utility>
#include <typeinfo>
#include <string>
#include <cassert>
#include <map>
#include <vector>
#include <list>
#include <memory>
#include <cstdint>

#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>
#include <Basics/Thread.h>
#include <Basics/ConditionVariable.h>
#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {

enum NodeType {NODE, LEAF};

using namespace arangodb::velocypack;

class StoreException : public std::exception {
public:
  explicit StoreException(std::string const& message) : _message(message) {}
  virtual char const* what() const noexcept override final { return _message.c_str(); }
private:
  std::string _message;
};

enum NODE_EXCEPTION {PATH_NOT_FOUND};

class Node;

typedef std::chrono::system_clock::time_point TimePoint;
typedef std::map<TimePoint, std::shared_ptr<Node>> TimeTable;
typedef std::map<std::shared_ptr<Node>, TimePoint> TableTime;

/// @brief Simple tree implementation
class Node {
  
public:

  // @brief Slash-segemented path 
  typedef std::vector<std::string> PathType;

  // @brief Child nodes
  typedef std::map<std::string, std::shared_ptr<Node>> Children;
  
  /// @brief Construct with name
  explicit Node (std::string const& name);

  /// @brief Construct with name and introduce to tree under parent
  Node (std::string const& name, Node* parent);

  /// @brief Default dtor
  virtual ~Node ();

  /// @brief Get name 
  std::string const& name() const;

  /// @brief Get full path
  std::string uri() const;

  /// @brief Apply rhs to this node (deep copy of rhs)
  Node& operator= (Node const& node);

  /// @brief Apply value slice to this node
  Node& operator= (arangodb::velocypack::Slice const&);

  /// @brief Check equality with slice
  bool operator== (arangodb::velocypack::Slice const&) const;

  /// @brief Type of this node (LEAF / NODE)
  NodeType type() const;

  /// @brief Get child specified by name
  Node& operator [](std::string name);
  Node const& operator [](std::string name) const;

  /// @brief Get node specified by path vector  
  Node& operator ()(std::vector<std::string>& pv);
  Node const& operator ()(std::vector<std::string>& pv) const;
  
  /// @brief Get node specified by path string  
  Node& operator ()(std::string const& path);
  Node const& operator ()(std::string const& path) const;

  /// @brief Remove node at absolut path
  bool remove (std::string const& path);

  /// @brief Remove child by name
  bool removeChild (std::string const& key);

  /// @brief Remove this node and below from tree
  bool remove();

  /// @brief Get root node
  Node& root();

  /// @brief Dump to ostream
  std::ostream& print (std::ostream&) const;

  /// #brief Get path of this node
  std::string path (); 
  
  /// @brief Apply single slice
  bool applies (arangodb::velocypack::Slice const&);

  /// @brief Create Builder representing this store
  void toBuilder (Builder&) const;

  /// @brief Create slice from value
  Slice slice() const;

  /// @brief Get value type  
  ValueType valueType () const;

protected:

  /// @brief Add time to live entry
  virtual bool addTimeToLive (long millis);

  /// @brief Remove time to live entry
  virtual bool removeTimeToLive ();
  
  Node* _parent;
  Children _children;
  TimeTable _time_table;
  TableTime _table_time;
  Buffer<uint8_t> _value;
  
  std::string _node_name;
  
};

inline std::ostream& operator<< (std::ostream& o, Node const& n) {
  return n.print(o);
}

class Agent;

/// @brief Key value tree 
class Store : public Node, public arangodb::Thread {
  
public:

  /// @brief Construct with name
  explicit Store (std::string const& name = "root");

  /// @brief Destruct
  virtual ~Store ();

  /// @brief Apply entry in query 
  std::vector<bool> apply (query_t const& query);

  /// @brief Apply entry in query 
  std::vector<bool> apply (std::vector<Slice> const& query);

  /// @brief Read specified query from store
  std::vector<bool> read (query_t const& query, query_t& result) const;
  
  /// @brief Begin shutdown of thread
  void beginShutdown () override final;

  /// @brief Start thread
  bool start ();

  /// @brief Start thread with access to agent
  bool start (Agent*);

  /// @brief Set name
  void name (std::string const& name);

  /// @brief Dump everything to builder
  void dumpToBuilder (Builder&) const;

private:
  /// @brief Read individual entry specified in slice into builder
  bool read  (arangodb::velocypack::Slice const&,
              arangodb::velocypack::Builder&) const;

  /// @brief Check precondition
  bool check (arangodb::velocypack::Slice const&) const;

  /// @brief Clear entries, whose time to live has expired
  void clearTimeTable ();

  /// @brief Run thread
  void run () override final;

  /// @brief Condition variable guarding removal of expired entries
  arangodb::basics::ConditionVariable _cv;

  /// @brief Read/Write mutex on database
  mutable arangodb::Mutex _storeLock;

  Agent* _agent;
  
};

}}

#endif
