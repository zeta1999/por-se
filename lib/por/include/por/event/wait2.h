#pragma once

#include "base.h"

#include "por/unfolding.h"

#include <cassert>
#include <array>

namespace por::event {
	class wait2 final : public event {
		// predecessors:
		// 1. same-thread predecessor (wait1)
		// 2. signal or broadcast that notified this event
		// 3. previous release of this lock
		std::array<event const*, 3> _predecessors;

		cond_id_t _cid;

		exploration_info _info;

	protected:
		wait2(thread_id_t tid,
			cond_id_t cid,
			event const& thread_predecessor,
			event const& lock_predecessor,
			event const& condition_variable_predecessor
		)
			: event(event_kind::wait2, tid, thread_predecessor, lock_predecessor, &condition_variable_predecessor)
			, _predecessors{&thread_predecessor, &condition_variable_predecessor, &lock_predecessor}
			, _cid(cid)
		{
			assert(this->thread_predecessor());
			assert(this->thread_predecessor()->tid());
			assert(this->thread_predecessor()->tid() == this->tid());
			assert(this->thread_predecessor()->kind() == event_kind::wait1);

			assert(this->lock_predecessor());
			assert(this->lock_predecessor()->kind() == event_kind::lock_release || this->lock_predecessor()->kind() == event_kind::wait1);

			assert(this->cid());

			assert(this->notifying_event());
			assert(this->notifying_event()->tid() != this->tid());
			if(this->notifying_event()->kind() == event_kind::signal) {
				[[maybe_unused]] auto sig = static_cast<signal const*>(this->notifying_event());
				assert(sig->notified_thread() == this->tid());
				assert(sig->cid() == this->cid());
				assert(sig->wait_predecessor() == this->thread_predecessor());
			} else {
				assert(this->notifying_event()->kind() == event_kind::broadcast);
				[[maybe_unused]] auto bro = static_cast<broadcast const*>(this->notifying_event());
				assert(bro->is_notifying_thread(this->tid()));
				assert(bro->cid() == this->cid());
				[[maybe_unused]] bool wait1_found = false;
				for(auto& e : bro->wait_predecessors()) {
					if(e == this->thread_predecessor()) {
						wait1_found = true;
						break;
					}
				}
				assert(wait1_found && "notifying broadcast must wake up corresponding wait1");
			}
		}

	public:
		static event const& alloc(
			unfolding& unfolding,
			thread_id_t tid,
			cond_id_t cid,
			event const& thread_predecessor,
			event const& lock_predecessor,
			event const& condition_variable_predecessor
		) {
			return unfolding.deduplicate(wait2{
				tid,
				cid,
				thread_predecessor,
				lock_predecessor,
				condition_variable_predecessor
			});
		}

		void mark_as_open(path_t const& path) const override {
			_info.mark_as_open(path);
		}
		void mark_as_explored(path_t const& path) const override {
			_info.mark_as_explored(path);
		}
		bool is_present(path_t const& path) const override {
			return _info.is_present(path);
		}
		bool is_explored(path_t const& path) const override {
			return _info.is_explored(path);
		}

		std::string to_string(bool details) const override {
			if(details)
				return "[tid: " + tid().to_string() + " depth: " + std::to_string(depth()) + " kind: wait2 cid: " + std::to_string(cid()) + "]";
			return "wait2";
		}

		util::iterator_range<event const* const*> predecessors() const override {
			return util::make_iterator_range<event const* const*>(_predecessors.data(), _predecessors.data() + _predecessors.size());
		}

		event const* thread_predecessor() const override {
			return _predecessors[0];
		}

		event const* lock_predecessor() const noexcept override { return _predecessors[2]; }

		util::iterator_range<event const* const*> condition_variable_predecessors() const noexcept {
			return util::make_iterator_range<event const* const*>(_predecessors.data(), _predecessors.data() + 2);
		}

		cond_id_t cid() const noexcept override { return _cid; }

		event const* notifying_event() const noexcept { return _predecessors[1]; }
	};
}
