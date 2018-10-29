#pragma once

#include "base.h"

#include <cassert>

namespace por::event {
	class lock_acquire final : public event {
		// predecessors:
		// 1. same-thread predecessor
		// 2. previous release of this lock
		std::array<std::shared_ptr<event>, 2> _predecessors;

	protected:
		lock_acquire(thread_id_t tid, std::shared_ptr<event> const& thread_predecessor, std::shared_ptr<event> const& lock_predecessor)
		: event(event_kind::lock_acquire, tid)
		, _predecessors{thread_predecessor, lock_predecessor}
		{
			assert(thread_dependency());
			assert(thread_dependency()->tid() != 0);
			assert(thread_dependency()->tid() == this->tid());
			assert(thread_dependency()->kind() != event_kind::program_start);
			assert(thread_dependency()->kind() != event_kind::thread_stop);
			assert(lock_dependency()->kind() == event_kind::lock_create || lock_dependency()->kind() == event_kind::lock_release);
		}

	public:
		static std::shared_ptr<lock_acquire> alloc(thread_id_t tid, std::shared_ptr<event> const& thread_predecessor, std::shared_ptr<event> const& lock_predecessor) {
			return std::make_shared<lock_acquire>(lock_acquire{tid, thread_predecessor, lock_predecessor});
		}

		virtual util::iterator_range<std::shared_ptr<event>*> predecessors() override {
			return util::make_iterator_range<std::shared_ptr<event>*>(_predecessors.data(), _predecessors.data() + _predecessors.size());
		}

		std::shared_ptr<event>      & thread_dependency()       noexcept { return _predecessors[0]; }
		std::shared_ptr<event> const& thread_dependency() const noexcept { return _predecessors[0]; }

		std::shared_ptr<event>      & lock_dependency()       noexcept { return _predecessors[1]; }
		std::shared_ptr<event> const& lock_dependency() const noexcept { return _predecessors[1]; }
	};
}
