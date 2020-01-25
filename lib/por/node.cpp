#include "include/por/node.h"

#include "include/por/configuration.h"
#include "include/por/event/event.h"

#include "util/check.h"

#include <algorithm>
#include <cassert>
#include <sstream>

#ifdef LIBPOR_KLEE
namespace klee {
	std::size_t klee_state_id(klee::ExecutionState const*);
}
#endif

using namespace por;

namespace {
	using atomic_pair_map_t = std::map<por::event::event const*, por::event::event const*>;

	static void extract_atomic_pairs(por::comb& comb, atomic_pair_map_t& map) {
		for(auto& [tid, thread] : comb.threads()) {
			for(auto& event : thread) {
				if(event->ends_atomic_operation()) {
					auto pred = event->atomic_predecessor();
					map.emplace(pred, event);
				}
			}
		}
		// beware of iterator invalidation in comb
		for(auto& [k, v] : map) {
			comb.remove(*v);
		}
		libpor_check(std::all_of(map.begin(), map.end(), [&comb](auto& entry) {
			return std::find(comb.begin(), comb.end(), entry.first) != comb.end();
		}));
	}
}

node* node::make_left_child(std::function<registration_t(por::configuration*)> func) {
	assert(!_left && "node already has left child");
	assert(!_event && "node must not have an event yet");

	_left = allocate_left_child();
	std::shared_ptr<state const> standby_state;

	if(_left->_C) {
		std::tie(_event, standby_state) = func(_left->_C.get());
	} else {
		std::tie(_event, standby_state) = func(nullptr);
	}

	if(standby_state) {
		_left->_standby_state = std::move(standby_state);
	}

	libpor_check(std::find(_D.begin(), _D.end(), _event) == _D.end());

	// propagate sweep bit
	_left->_is_sweep_node = _is_sweep_node;
	_is_sweep_node = false;

	return _left.get();
}

node* node::make_left_child(std::function<registration_t(por::configuration&)> func) {
	assert(_C);
	return make_left_child([&func](por::configuration* cfg) {
		return func(*cfg);
	});
}

node* node::make_right_local_child(std::function<registration_t(por::configuration&)> func) {
	assert(_event);
	assert(_event->kind() == por::event::event_kind::local);

	node* n = make_right_child();
	return n->make_left_child(std::move(func));
}

node* node::make_right_child() {
	assert(!_right && "node already has right child");
	assert(_event && "no event attached to node");

	auto D = _D;
	D.emplace_back(_event);
	_right = allocate_right_child(D);

	_right->_standby_state = _standby_state;

	return _right.get();
}

node* node::catch_up(std::function<node::registration_t(por::configuration&)> func, por::event::event const* next) {
	por::configuration copy = *_C;

	auto [event, standby_state] = func(copy);

	assert(next == event);

	node* n = this;
	while(n->_event != event && n->_event != nullptr) {
		n = n->right_child();
	}
	assert(n->_event == event);
	assert(n->_left);
	n = n->left_child();

	if(!n->_C) {
		n->_C = std::make_shared<por::configuration>(std::move(copy));
	} else {
		assert(copy.size() == n->_C->size());

		libpor_check(copy.thread_heads() == n->_C->thread_heads());
		libpor_check(copy.lock_heads() == n->_C->lock_heads());
		libpor_check(copy.cond_heads() == n->_C->cond_heads());
	}
	libpor_check(std::find(n->_D.begin(), n->_D.end(), event) == n->_D.end());

	if(standby_state && !n->_standby_state) {
		n->_standby_state = standby_state;
	}

	return n;
}

por::leaf node::make_right_branch(por::comb A) {
	assert(_event && "no event attached to node");

	// FIXME: root node includes configuration with both program_init and thread_init event
	assert(parent() && "cannot be called on root node");

	node* n = make_right_child();
	std::deque<por::event::event const*> catch_up;

	node* s = this;
	// search for closest node with standby state in current branch
	while(!s->_standby_state && s->parent()) {
		s = s->parent();
	}
	assert(s != nullptr);

	auto s_last = s->last_included_event();
	for(auto& r : rschedule()) {
		if(r == s_last) {
			break;
		}
		catch_up.push_front(r);
	}

	atomic_pair_map_t atomic_pairs;
	extract_atomic_pairs(A, atomic_pairs);

	while(!A.empty()) {
		A.sort();
		std::vector<por::event::event const*> min = A.min();

		// schedule events that form an atomic operation immediately following each other
		for(auto it = min.begin(); it != min.end();) {
			auto atom = atomic_pairs.find(*it);
			if(atom != atomic_pairs.end()) {
				++it;
				it = min.insert(it, atom->second);
				atomic_pairs.erase(atom);
			} else {
				++it;
			}
		}

		for(por::event::event const* a : min) {
			n = n->make_left_child([&a, &n](por::configuration*) {
				return std::make_pair(a, nullptr);
			});
			assert(n->parent()->_event == a);
			n->_C.reset();
			catch_up.push_back(a);
		}

		A.remove(min.begin(), min.end());
	}

	return {s, std::move(catch_up)};
}

std::vector<por::leaf> node::create_right_branches(std::vector<node*> B) {
	std::vector<por::leaf> leaves;

	if(!B.empty() && B.size() != (B.front()->configuration().size() - 1)) {
		return leaves;
	}

	for(auto n : B) {
		if(n->right_child()) {
			continue;
		}

		if(n->_event->immediate_conflicts().empty()) {
			continue;
		}
		libpor_check(n->_event->kind() != por::event::event_kind::thread_create);
		libpor_check(n->_event->kind() != por::event::event_kind::thread_init);
		libpor_check(n->_event->kind() != por::event::event_kind::lock_release);

		if(n->_event->ends_atomic_operation()) {
			// event always has to follow its predecessor, it can not be excluded independently
			continue;
		}
		if(n->_event->kind() == por::event::event_kind::local) {
			// do not compute alternatives to local events, this is done by KLEE
			continue;
		}
		if(n->_event->kind() == por::event::event_kind::thread_init) {
			// thread_init always has to immediately succeed its thread_create
			// there is no (unique) alternative to be found here
			continue;
		}

		assert(n->_event != nullptr);
		por::configuration const& cfg = n->configuration();
		auto D = n->_D;
		D.push_back(n->_event);
		por::event::event const* j = cfg.compute_alternative(std::move(D));

		if(j == nullptr) {
			continue;
		}

		// compute A := [j] \setminus _C
		por::cone J(*j);
		por::cone C(cfg);
		por::comb A = J.setminus(C);
		libpor_check(A.is_sorted());

		std::vector<por::event::event const*> atomic_successors;

		// make sure to include thread_exit if immediately preceding lock_release is in A
		for(auto& l : A) {
			if(l->kind() == por::event::event_kind::lock_release) {
				auto exit = std::find_if(l->successors().begin(), l->successors().end(), [&l](auto& succ) {
					return succ->kind() == por::event::event_kind::thread_exit;
				});
				if(exit != l->successors().end()) {
					atomic_successors.push_back(*exit);
				}
			}
		}

		// make sure to include thread_init if thread_create is in A
		for(auto& l : A) {
			if(l->kind() == por::event::event_kind::thread_create) {
				auto init = std::find_if(l->successors().begin(), l->successors().end(), [&l](auto& succ) {
					return succ->kind() == por::event::event_kind::thread_init;
				});
				if(init != l->successors().end()) {
					atomic_successors.push_back(*init);
				}
			}
		}

		for(auto& e : atomic_successors) {
			A.insert(*e);
		}

		leaves.push_back(n->make_right_branch(std::move(A)));
	}
	assert(B.empty() || B.size() == (B.front()->configuration().size() - 1));

	return leaves;
}

void node::update_sweep_bit() {
	assert(_left == nullptr && _right != nullptr);
	node* n = right_child();
	while(n->left_child()) {
		n = n->left_child();
	}
	n->_is_sweep_node = true;
	assert(!_is_sweep_node);
}

void node::backtrack() {
	assert(!has_children());
	if(!_is_sweep_node) {
		return;
	}
	node* n = this;
	while(n && !n->has_children()) {
		node* p = n->parent();

		// remove n
		if(p && p->left_child() == n) {
			p->_left.reset();
		} else if(p && p->right_child() == n) {
			p->_right.reset();
		}

		n = p;
	}
	if(!n) {
		return;
	}
	n->update_sweep_bit();
}

std::string node::to_string(bool with_schedule) const noexcept {
	std::ostringstream result;
	result << "node " << this << "\n";

	result << "branch:\n";
	std::vector<node const*> br(branch_begin(), branch_end());
	std::reverse(br.begin(), br.end());
	for(node const* n : br) {
		result << "  node: " << n << "\n";
		result << "    parent: ";
		if(!n->parent()) {
			result << "nullptr\n";
		} else {
			result << n->parent();
			if(n->parent()->left_child() == n) {
				result << " (left child)\n";
			} else if(n->parent()->right_child() == n) {
				result << " (right child)\n";
			}
		}
		result << "    event: ";
		if(!n->_event) {
			result << "nullptr\n";
		} else {
			result << n->_event->to_string(true) << " @ " << n->_event << "\n";
		}
		if(!n->_standby_state) {
			result << "    standby state: nullptr\n";
		} else {
#ifdef LIBPOR_KLEE
			result << "    standby state id: ";
			result << klee::klee_state_id(n->_standby_state.get()) << " (" << n->_standby_state << ")\n";
#else
			result << "    standby state: ";
			result << n->_standby_state << "\n";
#endif
		}
		result << "    is_sweep_node: " << n->_is_sweep_node << "\n";
		result << "    |C| = " << std::to_string(n->configuration().size()) << "\n";
		result << "    |D| = " << std::to_string(n->_D.size()) << "\n";
		for(auto& d : n->_D) {
			result << "      " << d->to_string(true) << " @ " << d << "\n";
		}
	}

	if(with_schedule) {
		auto sched = schedule();
		if(!sched.empty()) {
			result << "schedule:\n";
			for(auto* e : sched) {
				assert(e);
				result << "  " << e->to_string(true) << " @ " << e << "\n";
			}
		}
	}

	return result.str();
}
