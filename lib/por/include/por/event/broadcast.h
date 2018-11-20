#pragma once

#include "base.h"

#include <cassert>
#include <memory>
#include <array>

namespace por::event {
	class broadcast final : public event {
		// predecessors:
		// 1. same-thread predecessor
		// 2. previous broadcast operation
		std::array<std::shared_ptr<event>, 2> _predecessors;

	protected:
		template<typename T>
		broadcast(thread_id_t tid,
			std::shared_ptr<event>&& thread_predecessor,
			std::shared_ptr<event>&& condition_variable_predecessor
		)
			: event(event_kind::broadcast, tid)
			, _predecessors{std::move(thread_predecessor), std::move(condition_variable_predecessor)}
		{
			assert(this->thread_predecessor());
			assert(this->thread_predecessor()->tid() != 0);
			assert(this->thread_predecessor()->tid() == this->tid());
			assert(this->thread_predecessor()->kind() != event_kind::program_init);
			assert(this->thread_predecessor()->kind() != event_kind::thread_exit);
			assert(this->condition_variable_predecessor());
		}

	public:
		template<typename T>
		static std::shared_ptr<broadcast> alloc(thread_id_t tid,
			std::shared_ptr<event> thread_predecessor,
			std::shared_ptr<event> condition_variable_predecessor
		) {
			return std::make_shared<broadcast>(broadcast{tid,
				std::move(thread_predecessor),
				std::move(condition_variable_predecessor)
			});
		}

		virtual util::iterator_range<std::shared_ptr<event>*> predecessors() override {
			return util::make_iterator_range<std::shared_ptr<event>*>(_predecessors.data(), _predecessors.data() + _predecessors.size());
		}

		std::shared_ptr<event>      & thread_predecessor()       noexcept { return _predecessors[0]; }
		std::shared_ptr<event> const& thread_predecessor() const noexcept { return _predecessors[0]; }

		std::shared_ptr<event>      & condition_variable_predecessor()       noexcept { return _predecessors[1]; }
		std::shared_ptr<event> const& condition_variable_predecessor() const noexcept { return _predecessors[1]; }
	};
}
