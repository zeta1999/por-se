#pragma once

#include "base.h"

namespace por::event {
	class thread_create final : public event {
		// predecessors:
		// 1. same-thread predecessor
		std::array<std::shared_ptr<event>, 1> _predecessors;

	protected:
		thread_create(thread_id_t tid, std::shared_ptr<event> const& thread_predecessor)
		: event(event_kind::thread_create, tid)
		, _predecessors{thread_predecessor}
		{
			assert(thread_dependency());
			assert(thread_dependency()->tid() != 0);
			assert(thread_dependency()->tid() == this->tid());
			assert(thread_dependency()->kind() != event_kind::program_start);
			assert(thread_dependency()->kind() != event_kind::thread_stop);
		}

	public:
		static std::shared_ptr<thread_create> alloc(thread_id_t tid, std::shared_ptr<event> const& thread_predecessor) {
			return std::make_shared<thread_create>(thread_create{tid, thread_predecessor});
		}

		virtual util::iterator_range<std::shared_ptr<event>*> predecessors() override {
			return util::make_iterator_range<std::shared_ptr<event>*>(_predecessors.data(), _predecessors.data() + _predecessors.size());
		}

		std::shared_ptr<event>      & thread_dependency()       noexcept { return _predecessors[0]; }
		std::shared_ptr<event> const& thread_dependency() const noexcept { return _predecessors[0]; }
	};
}
