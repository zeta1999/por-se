#pragma once

#include "por/configuration.h"

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace klee {
	class ExecutionState;
}

namespace por {
	class configuration;
	class unfolding;

	namespace event {
		class event;
	}

	using event_set_t = std::vector<por::event::event const*>;

	template<typename V>
	class node_branch_iterator {
	public:
		using value_type = V;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type const*;
		using reference = value_type const&;

	private:
		value_type _branch = nullptr;
		value_type _node = nullptr;

	public:
		using iterator_category = std::forward_iterator_tag;

		node_branch_iterator() = default;
		explicit node_branch_iterator(std::remove_pointer_t<value_type>& branch, bool end=false) : _branch(&branch) {
			if(!end) {
				_node = &branch;
			}
		}

		reference operator*() const noexcept { return _node; }
		pointer operator->() const noexcept { return &_node; }

		node_branch_iterator& operator++() noexcept {
			if(!_node) {
				return *this;
			}

			do {
				_node = _node->parent();
			} while(_node && _node->parent() && _node->is_right_child());
			assert(!_node || _node->is_left_child());

			return *this;
		}
		node_branch_iterator operator++(int) noexcept {
			node_branch_iterator tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const node_branch_iterator& rhs) const noexcept {
			return _branch == rhs._branch && _node == rhs._node;
		}
		bool operator!=(const node_branch_iterator& rhs) const noexcept {
			return !(*this == rhs);
		}
	};

	class node {
		class passkey {
			passkey() {}
			friend node;
		};

		node* _parent = nullptr;
		por::event::event const* _event = nullptr;

		// IMPORTANT: configuration and standby state always need to correspond to each other
		std::shared_ptr<por::configuration> _C; // right node has same configuration
		std::shared_ptr<klee::ExecutionState const> _standby_state;
		node* _catch_up_ptr = nullptr; // pointer to earliest node in current branch that needs catch-up

		event_set_t _D;
		std::unique_ptr<node> _left, _right;
		bool _is_sweep_node = false;

		std::unique_ptr<node> allocate_left_child() {
			return std::make_unique<node>(passkey{}, this, std::make_shared<por::configuration>(*_C), _D);
		}

		std::unique_ptr<node> allocate_right_child(event_set_t D) {
			return std::make_unique<node>(passkey{}, this, _C, D);
		}

	public:
		node(passkey, node* parent, std::shared_ptr<por::configuration> C, event_set_t D)
		: _parent(parent), _C(std::move(C)), _D(D) { }

		// root constructor
		explicit node() : _C(std::make_shared<por::configuration>()), _D({}), _is_sweep_node(true) { }

		node(const node&) = delete;
		node(node&&) = delete;
		node& operator=(const node&) = delete;
		node& operator=(node&&) = delete;
		~node() = default;

		por::configuration const& configuration() const noexcept {
			assert(_C);
			return *_C.get();
		}

		node* parent() noexcept {
			return _parent;
		}

		node const* parent() const noexcept {
			return _parent;
		}

		bool has_children() const noexcept {
			return _left || _right;
		}

		node* left_child() noexcept {
			return _left.get();
		}

		node const* left_child() const noexcept {
			return _left.get();
		}

		node* right_child() noexcept {
			return _right.get();
		}

		node const* right_child() const noexcept {
			return _right.get();
		}

		bool is_left_child() const noexcept {
			return !parent() || parent()->left_child() == this;
		}

		bool is_right_child() const noexcept {
			return !parent() || parent()->right_child() == this;
		}

		bool has_event() const noexcept {
			return _event != nullptr;
		}

		klee::ExecutionState const* standby_state() const noexcept {
			return _standby_state.get();
		}

		using registration_t = std::pair<por::event::event const*, std::shared_ptr<klee::ExecutionState const>>;

		node* make_left_child(std::function<registration_t(por::configuration&)>);

		void create_right_branches(std::vector<por::node*>);

		void catch_up(std::function<registration_t(por::configuration&)>);

		void backtrack();

		bool needs_catch_up() const noexcept;

		por::event::event const* peek() const noexcept;

		auto branch_begin() const noexcept { return node_branch_iterator<node const*>(*this); }
		auto branch_end() const noexcept { return node_branch_iterator<node const*>(*this, true); }

		auto branch_cbegin() const noexcept { return node_branch_iterator<node const*>(*this); }
		auto branch_cend() const noexcept { return node_branch_iterator<node const*>(*this, true); }

		auto branch_begin() noexcept { return node_branch_iterator<node*>(*this); }
		auto branch_end() noexcept { return node_branch_iterator<node*>(*this, true); }

		auto branch() noexcept { return util::make_iterator_range(branch_begin(), branch_end()); }
		auto branch() const noexcept { return util::make_iterator_range(branch_begin(), branch_end()); }
		auto cbranch() const noexcept { return util::make_iterator_range(branch_begin(), branch_end()); }

		std::vector<por::event::event const*> schedule() const noexcept {
			std::vector<por::event::event const*> sched;
			node const* n = this;
			while(n->parent()) {
				assert(n->has_event());
				sched.push_back(n->_event);
				n = n->parent();
			}
			return sched;
		}

		std::size_t distance_to_last_standby_state() const noexcept {
			std::size_t result = 0;
			node const* n = this;
			while(n->parent() && !n->_standby_state) {
				++result;
				n = n->parent();
			}
			return result;
		}

	private:
		node* make_right_child();

		node* make_right_branch(por::comb);

		void update_sweep_bit();
	};
}
