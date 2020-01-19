#pragma once

#include "base.h"

#include "por/unfolding.h"

#include <cassert>
#include <array>

namespace por::event {
	class thread_join final : public event {
		// predecessors:
		// 1. same-thread predecessor
		// 2. joined predecessor
		std::array<event const*, 2> _predecessors;

	protected:
		thread_join(thread_id_t tid, event const& thread_predecessor, event const& joined_predecessor)
			: event(event_kind::thread_join, tid, thread_predecessor, &joined_predecessor)
			, _predecessors{&thread_predecessor, &joined_predecessor}
		{
			assert(this->thread_predecessor());
			assert(this->thread_predecessor()->tid());
			assert(this->thread_predecessor()->tid() == this->tid());
			assert(this->thread_predecessor()->kind() != event_kind::program_init);
			assert(this->thread_predecessor()->kind() != event_kind::thread_exit);
			assert(this->joined_thread_predecessor());
			assert(this->joined_thread_predecessor()->tid());
			assert(this->joined_thread_predecessor()->tid() != this->tid());
			assert(this->joined_thread_predecessor()->kind() == event_kind::thread_exit);
		}

	public:
		static por::unfolding::deduplication_result alloc(
			unfolding& unfolding,
			thread_id_t tid,
			event const& thread_predecessor,
			event const& joined_predecessor
		) {
			return unfolding.deduplicate(thread_join{
				tid,
				thread_predecessor,
				joined_predecessor
			});
		}

		thread_join(thread_join&& that)
		: event(std::move(that))
		, _predecessors(that._predecessors) {
			that._predecessors = {};
			for(auto& pred : immediate_predecessors_from_cone()) {
				assert(pred != nullptr);
				replace_successor_of(*pred, that);
			}
		}

		~thread_join() {
			assert(!has_successors());
			for(auto& pred : immediate_predecessors_from_cone()) {
				assert(pred != nullptr);
				remove_from_successors_of(*pred);
			}
		}

		thread_join() = delete;
		thread_join(const thread_join&) = delete;
		thread_join& operator=(const thread_join&) = delete;
		thread_join& operator=(thread_join&&) = delete;

		std::string to_string(bool details) const noexcept override {
			if(details)
				return "[tid: " + tid().to_string() + " depth: " + std::to_string(depth()) + " kind: thread_join with: " + joined_thread().to_string() + (is_cutoff() ? " CUTOFF" : "") + "]";
			return "thread_join";
		}

		util::iterator_range<event const* const*> predecessors() const noexcept override {
			if(_predecessors[0] == nullptr) {
				return util::make_iterator_range<event const* const*>(nullptr, nullptr); // only after move-ctor
			}
			return util::make_iterator_range<event const* const*>(_predecessors.data(), _predecessors.data() + _predecessors.size());
		}

		event const* thread_predecessor() const noexcept override {
			return _predecessors[0];
		}

		thread_id_t joined_thread() const noexcept { return _predecessors[1]->tid(); }

		event const* joined_thread_predecessor() const noexcept { return _predecessors[1]; }
	};
}
