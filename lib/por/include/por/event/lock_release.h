#pragma once

#include "base.h"

#include "por/unfolding.h"

#include <cassert>
#include <array>

namespace por::event {
	class lock_release final : public event {
		// predecessors:
		// 1. same-thread predecessor
		// 2. previous acquisition of this lock
		std::array<event const*, 2> _predecessors;

		lock_id_t _lid;

		bool _atomic;

	protected:
		lock_release(
			thread_id_t tid,
			lock_id_t lid,
			event const& thread_predecessor,
			event const& lock_predecessor,
			bool atomic
		)
			: event(event_kind::lock_release, tid, thread_predecessor, &lock_predecessor)
			, _predecessors{&thread_predecessor, &lock_predecessor}
			, _lid(lid)
			, _atomic(atomic)
		{
			assert(this->thread_predecessor());
			assert(this->thread_predecessor()->tid());
			assert(this->thread_predecessor()->tid() == this->tid());
			assert(this->thread_predecessor()->kind() != event_kind::program_init);
			assert(this->thread_predecessor()->kind() != event_kind::thread_exit);
			assert(this->lock_predecessor());
			assert(
				this->lock_predecessor()->kind() == event_kind::lock_acquire
				|| this->lock_predecessor()->kind() == event_kind::wait2
			);
			assert(this->lock_predecessor()->tid() == this->tid());
			assert(this->lock_predecessor()->lid() == this->lid());
			assert(this->lid());

			if(this->ends_atomic_operation()) {
				assert(this->atomic_predecessor());
				assert(this->atomic_predecessor() == this->lock_predecessor());
				assert(this->atomic_predecessor() == this->thread_predecessor());
				assert(this->atomic_predecessor()->kind() == event_kind::lock_acquire);
			} else {
				assert(this->atomic_predecessor() == nullptr);
			}
		}

	public:
		static event const& alloc(
			unfolding& unfolding,
			thread_id_t tid,
			lock_id_t lid,
			event const& thread_predecessor,
			event const& lock_predecessor,
			bool atomic = false
		) {
			return unfolding.deduplicate(lock_release{
				tid,
				lid,
				thread_predecessor,
				lock_predecessor,
				atomic
			});
		}

		lock_release(lock_release&& that)
		: event(std::move(that))
		, _predecessors(std::move(that._predecessors))
		, _lid(std::move(that._lid))
		, _atomic(that._atomic) {
			for(auto& pred : predecessors()) {
				assert(pred != nullptr);
				replace_successor_of(*pred, that);
			}
		}

		~lock_release() {
			assert(!has_successors());
			for(auto& pred : predecessors()) {
				assert(pred != nullptr);
				remove_from_successors_of(*pred);
			}
		}

		lock_release() = delete;
		lock_release(const lock_release&) = delete;
		lock_release& operator=(const lock_release&) = delete;
		lock_release& operator=(lock_release&&) = delete;

		std::string to_string(bool details) const noexcept override {
			if(details)
				return "[tid: " + tid().to_string() + " depth: " + std::to_string(depth()) + " kind: lock_release"
					+ (ends_atomic_operation() ? " (atomic)" : "") + " lid: " + std::to_string(lid()) + "]";
			return "lock_release";
		}

		util::iterator_range<event const* const*> predecessors() const noexcept override {
			return util::make_iterator_range<event const* const*>(_predecessors.data(), _predecessors.data() + _predecessors.size());
		}

		event const* thread_predecessor() const noexcept override {
			return _predecessors[0];
		}

		event const* lock_predecessor() const noexcept override { return _predecessors[1]; }

		lock_id_t lid() const noexcept override { return _lid; }

		bool ends_atomic_operation() const noexcept override { return _atomic; }

		event const* atomic_predecessor() const noexcept override {
			if(ends_atomic_operation()) {
				return lock_predecessor();
			}
			return nullptr;
		}
	};
}
