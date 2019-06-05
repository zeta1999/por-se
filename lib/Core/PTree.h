//===-- PTree.h -------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef __UTIL_PTREE_H__
#define __UTIL_PTREE_H__

#include <klee/Expr.h>
#include <klee/Thread.h>

#include <set>

namespace klee {
  class ExecutionState;

  typedef struct {
    Thread::ThreadId scheduledThread;
    uint64_t epochNumber;
  } SchedulingDecision;

  class PTree { 
    typedef ExecutionState* data_type;

  public:
    typedef class PTreeNode Node;
    Node *root;

    explicit PTree(const data_type &_root);
    ~PTree() = default;
    
    std::pair<Node*,Node*> split(Node *n,
                                 const data_type &leftData,
                                 const data_type &rightData);
    void remove(Node *n);

    void dump(llvm::raw_ostream &os);
  };

  class PTreeNode {
    friend class PTree;
  public:
    PTreeNode *parent = nullptr;
    PTreeNode *left = nullptr;
    PTreeNode *right = nullptr;
    ExecutionState *data = nullptr;

    SchedulingDecision schedulingDecision;

  private:
    PTreeNode(PTreeNode * parent, ExecutionState * data);
    ~PTreeNode() = default;
  };
}

#endif
