#include <por/configuration.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <random>
#include <iostream>
#include <iomanip>
#include <set>

#include <util/sso_array.h>

namespace {
	por::event::thread_id_t choose_thread(por::configuration const& configuration, std::mt19937_64& gen) {
		std::uniform_int_distribution<std::size_t> dis(1, configuration.active_threads());
		std::size_t const chosen = dis(gen);
		std::size_t count = 0;
		for(auto it = configuration.thread_heads().begin(); ; ++it){
			assert(it != configuration.thread_heads().end());
			if(it->second->kind() != por::event::event_kind::thread_exit && it->second->kind() != por::event::event_kind::wait1) {
				++count;
				if(count == chosen) {
					assert(it->first == it->second->tid());
					return it->first;
				}
			}
		}
		assert(false && "Active thread number was too large");
		std::abort();
	}

	por::event::thread_id_t choose_suitable_thread(por::configuration const& configuration,
		std::mt19937_64& gen,
		std::bernoulli_distribution& rare_choice,
		por::event::event_kind kind
	) {
		for(bool done = false; !done; ) {
			unsigned count = 0;
			for(auto const& t : configuration.thread_heads()) {
				if(t.second->kind() != kind)
					continue;

				++count;
				if(rare_choice(gen)) {
					return t.first;
				}
			}
			if(count == 0) {
				// no suitable threads exist
				done = true;
			}
		}
		return 0;
	}

	por::event::lock_id_t choose_lock(por::configuration const& configuration, std::mt19937_64& gen) {
		if(configuration.lock_heads().empty())
			return 0;

		std::uniform_int_distribution<std::size_t> dis(0, configuration.lock_heads().size() - 1);
		std::size_t chosen = dis(gen);
		return std::next(configuration.lock_heads().begin(), chosen)->first;
	}

	por::event::lock_id_t choose_suitable_lock(por::configuration const& configuration,
		std::mt19937_64& gen,
		std::bernoulli_distribution& rare_choice,
		bool released,
		por::event::lock_id_t locked_by_tid = 0
	) {
		for(bool done = false; !done; ) {
			unsigned count = 0;
			for(auto const& l : configuration.lock_heads()) {
				auto lock_kind = l.second->kind();
				bool suitable = false;
				if(released) {
					suitable |= lock_kind == por::event::event_kind::lock_create;
					suitable |= lock_kind == por::event::event_kind::lock_release;
					suitable |= lock_kind == por::event::event_kind::wait1;
				} else {
					suitable |= lock_kind == por::event::event_kind::lock_acquire;
					suitable |= lock_kind == por::event::event_kind::wait2;
					if(suitable && locked_by_tid)
						suitable = l.second->tid() == locked_by_tid;
					if(suitable)
						suitable = configuration.thread_heads().find(l.second->tid())->second->kind() != por::event::event_kind::thread_exit;
				}

				if(!suitable)
					continue;

				++count;
				if(rare_choice(gen)) {
					return l.first;
				}
			}
			if(count == 0) {
				// no suitable locks exist
				done = true;
			}
		}
		return 0;
	}

	por::event::cond_id_t choose_cond(por::configuration const& configuration, std::mt19937_64& gen) {
		if(configuration.cond_heads().empty())
			return 0;

		std::uniform_int_distribution<std::size_t> dis(0, configuration.cond_heads().size() - 1);
		std::size_t chosen = dis(gen);
		return std::next(configuration.cond_heads().begin(), chosen)->first;
	}

	por::event::cond_id_t choose_suitable_cond(por::configuration const& configuration,
		std::mt19937_64& gen,
		std::bernoulli_distribution& rare_choice,
		bool blocked
	) {
		for(bool done = false; !done; ) {
			unsigned count = 0;
			for(auto const& c : configuration.cond_heads()) {
				std::size_t num_blocked = std::count_if(c.second.begin(), c.second.end(), [](auto& e) { return e->kind() == por::event::event_kind::wait1; });
				if((!blocked && num_blocked > 0) || (blocked && num_blocked == 0))
					continue;

				++count;
				if(rare_choice(gen)) {
					return c.first;
				}
			}
			if(count == 0) {
				// no suitable conds exist
				done = true;
			}
		}
		return 0;
	}
}

int main(int argc, char** argv){
	assert(argc > 0);

	por::configuration configuration; // construct a default configuration with 1 main thread
	por::event::thread_id_t next_thread_id = 2;
	por::event::lock_id_t next_lock_id = 1;
	por::event::lock_id_t next_cond_id = 1;

#ifdef SEED
	std::mt19937_64 gen(SEED);
#else
	std::mt19937_64 gen(35);
#endif

	if(argc > 1) {
		gen.seed(std::stoi(argv[1]));
	}

	// "warm up" mersenne twister to deal with weak initialization function
	for(unsigned i = 0; i < 10'000; ++i) {
		static_cast<void>(gen());
	}
	std::uniform_int_distribution<unsigned> event_dis(0, 999);
	std::bernoulli_distribution rare_choice(0.1);

	while(configuration.active_threads() > 0) {
		auto const roll = event_dis(gen);
		std::cout << "   r " << std::setw(3) << roll << "\n";
		if(roll < 40) {
			// spawn new thread
			auto source = choose_thread(configuration, gen);
			auto tid = next_thread_id++;
			configuration.spawn_thread(source, tid);
			std::cout << "+T " << tid << " (" << source << ")\n";
		} else if(roll < 60) {
			// join thread
			auto tid = choose_thread(configuration, gen);
			auto join_tid = choose_suitable_thread(configuration, gen, rare_choice, por::event::event_kind::thread_exit);
			if(tid && join_tid) {
				configuration.join_thread(tid, join_tid);
				std::cout << "jT " << tid << " " << join_tid << "\n";
				break;
			}
		} else if(roll < 100) {
			// kill old thread
			auto tid = choose_thread(configuration, gen);
			configuration.exit_thread(tid);
			std::cout << "-T " << tid << "\n";
		} else if(roll < 200) {
			// spawn new lock
			auto tid = choose_thread(configuration, gen);
			auto lid = next_lock_id++;
			configuration.create_lock(tid, lid);
			std::cout << "+L " << lid << " (" << tid << ")\n";
		} else if(roll < 300) {
			// destroy lock, if one exists
			auto lid = choose_suitable_lock(configuration, gen, rare_choice, true);
			auto tid = choose_thread(configuration, gen);
			if(lid && tid) {
				bool no_block_on_lock = true;
				for(auto& e : configuration.thread_heads()) {
					if(e.second->kind() == por::event::event_kind::wait1) {
						auto& w = e.second;
						auto* l = &configuration.lock_heads().at(lid);
						while(l != nullptr && *w < **l) {
							l = configuration.get_lock_predecessor(*l);
						}
						if(*l == w) {
							no_block_on_lock = false;
						}
					}
				}
				if(no_block_on_lock) {
					configuration.destroy_lock(tid, lid);
					std::cout << "-L " << lid << " (" << tid << ")\n";
				}
			}
		} else if(roll < 400) {
			// acquire lock, if one can be acquired
			auto lid = choose_suitable_lock(configuration, gen, rare_choice, true);
			auto tid = choose_thread(configuration, gen);
			if(lid && tid) {
				configuration.acquire_lock(tid, lid);
				std::cout << " L+ " << lid << " (" << tid << ")\n";
			}
		} else if(roll < 500) {
			// release lock, if one can be released
			auto lid = choose_suitable_lock(configuration, gen, rare_choice, false);
			if(lid) {
				auto const tid = configuration.lock_heads().find(lid)->second->tid();
				if(configuration.thread_heads().find(tid)->second->kind() != por::event::event_kind::wait1) {
					configuration.release_lock(tid, lid);
					std::cout << " L- " << lid << " (" << tid << ")\n";
				}
			}
		} else if(roll < 600) {
			// wait on condition variable, if possible
			auto tid = choose_thread(configuration, gen);
			auto lid = choose_suitable_lock(configuration, gen, rare_choice, false, tid);
			auto cid = choose_cond(configuration, gen);
			if(tid && lid && cid) {
				configuration.wait1(tid, cid, lid);
				std::cout << " C+ " << cid << ", " <<  lid << " (" << tid << ")\n";
			}
		} else if(roll < 700) {
			// signal single thread, if possible
			auto tid = choose_thread(configuration, gen);
			auto cid = choose_suitable_cond(configuration, gen, rare_choice, true);
			por::event::thread_id_t blocked_tid = 0;
			if(tid && cid) {
				for(auto& w : configuration.cond_heads().at(cid)) {
					if(w->kind() != por::event::event_kind::wait1 || w->tid() == tid)
						continue;
					blocked_tid = w->tid();
					break;
				}
				configuration.signal_thread(tid, cid, blocked_tid);
				std::cout << "sT " << cid << ", " <<  blocked_tid << " (" << tid << ")\n";
			}
		} else if(roll < 750) {
			// lost signal, if possible
			auto tid = choose_thread(configuration, gen);
			// TODO: can also happen with blocked cids
			auto cid = choose_suitable_cond(configuration, gen, rare_choice, false);
			if(tid && cid) {
				configuration.signal_thread(tid, cid, 0);
				std::cout << "sT " << cid << ", " <<  0 << " (" << tid << ")\n";
			}
		} else if(roll < 800) {
			// broadcast threads, if possible
			auto tid = choose_thread(configuration, gen);
			auto cid = choose_suitable_cond(configuration, gen, rare_choice, true);
			std::set<por::event::thread_id_t> blocked_tids;
			if(tid && cid) {
				for(auto& w : configuration.cond_heads().at(cid)) {
					if(w->kind() != por::event::event_kind::wait1 || w->tid() == tid)
						continue;
					blocked_tids.insert(w->tid());
					break;
				}
				configuration.broadcast_threads(tid, cid, blocked_tids);
				std::cout << "bT " << cid << ", " <<  blocked_tids.size() << " threads (" << tid << ")\n";
			}
		} else if(roll < 850) {
			// lost broadcast, if possible
			auto tid = choose_thread(configuration, gen);
			// TODO: can also happen with blocked cids
			auto cid = choose_suitable_cond(configuration, gen, rare_choice, false);
			if(tid && cid) {
				configuration.broadcast_threads(tid, cid, {});
				std::cout << "bT " << cid << ", {} (" << tid << ")\n";
			}
		} else if(roll < 900) {
			// wake up notified thread, if possible
			auto tid = choose_suitable_thread(configuration, gen, rare_choice, por::event::event_kind::wait1);
			if(tid) {
				auto& wait1 = configuration.thread_heads().find(tid)->second;
				por::event::cond_id_t cid = 0;
				for(auto& cond : configuration.cond_heads()) {
					for(auto& e : cond.second) {
						if(e->tid() == tid || (e->kind() != por::event::event_kind::signal && e->kind() != por::event::event_kind::broadcast))
							continue;
						if(e->kind() == por::event::event_kind::signal) {
							auto sig = static_cast<por::event::signal const*>(e.get());
							if(sig->wait_predecessor() == wait1) {
								cid = cond.first;
							}
						} else {
							assert(e->kind() == por::event::event_kind::broadcast);
							auto bro = static_cast<por::event::broadcast const*>(e.get());
							for(auto& w : bro->wait_predecessors()) {
								if(w == wait1) {
									cid = cond.first;
									break;
								}
							}
						}
						if(cid)
							break;
					}
					if(cid)
						break;
				}
				if(cid) {
					auto lid = 0;
					for(auto& e : configuration.lock_heads()) {
						auto l = &e.second;
						while(l != nullptr && *wait1 < **l) {
							l = configuration.get_lock_predecessor(*l);
						}
						if(*l == wait1) {
							lid = e.first;
						}
					}
					if(lid) {
						configuration.wait2(tid, cid, lid);
						std::cout << "wT " << cid << ", " <<  lid << " (" << tid << ")\n";
					}
				}
			}
		} else if(roll < 950) {
			// spawn new cond
			auto tid = choose_thread(configuration, gen);
			auto cid = next_cond_id++;
			configuration.create_cond(tid, cid);
			std::cout << "+C " << cid << " (" << tid << ")\n";
		} else if(roll < 970) {
			// destroy cond, if one exists
			auto tid = choose_thread(configuration, gen);
			auto cid = choose_suitable_cond(configuration, gen, rare_choice, false);
			if(cid) {
				configuration.destroy_cond(tid, cid);
				std::cout << "-C " << cid << " (" << tid << ")\n";
			}
		} else if(roll < 1000) {
			auto tid = choose_thread(configuration, gen);
			configuration.local(tid);
			std::cout << " . (" << tid << ")\n";
		} else {
			assert(false && "Unexpected random choice for event to introduce");
			std::abort();
		}
	}

	auto cex = configuration.conflicting_extensions();
	std::cerr << cex.size() << " cex found\n";
	for(auto& c : cex) {
		auto const* acq = static_cast<por::event::lock_acquire const*>(c.get());
		assert(acq->thread_predecessor());
		auto lp = acq->lock_predecessor();
		std::cerr << c->tid() << "@" << c->depth() << ": " << acq->thread_predecessor()->tid() << "@" << acq->thread_predecessor()->depth();
		if(lp) {
			std::cerr << " | " << (*lp).tid() << "@" << (*lp).depth() << "\n";
		} else {
			std::cerr << "\n";
		}
	}

	std::set<por::event::event const*> visited;
	std::vector<por::event::event const*> open;
	std::map<por::event::thread_id_t, std::vector<por::event::event const*>> threads;
	std::vector<std::pair<por::event::event const*, por::event::event const*>> inter_thread_dependencies;
	std::map<por::event::thread_id_t, std::vector<std::pair<por::event::event const*, por::event::event const*>>> non_immediate_intra_thread_dependencies;
	std::map<por::event::thread_id_t, std::vector<std::pair<por::event::event const*, por::event::event const*>>> intra_thread_dependencies;
	std::size_t opened = 0;
	for(auto& t : configuration.thread_heads()) {
		open.push_back(t.second.get());
		++opened;
	}
	while(!open.empty()) {
		por::event::event const* event = open.back();
		open.pop_back();
		if(!visited.insert(event).second) {
			// already visited
			continue;
		}
		por::event::thread_id_t tid = event->tid();
		threads[tid].push_back(event);
		bool first = true;
		for(auto& p : event->predecessors()) {
			por::event::event const* predecessor = p.get();
			if(visited.count(predecessor) == 0) {
				open.push_back(predecessor);
				++opened;
			}
			if(tid != predecessor->tid()) {
				inter_thread_dependencies.emplace_back(std::make_pair(event, predecessor));
			} else {
				if(first) {
					intra_thread_dependencies[tid].emplace_back(std::make_pair(event, predecessor));
					first = false; // only insert thread_predecessor, not lock or cond predecessor on same thread
				} else {
					non_immediate_intra_thread_dependencies[tid].emplace_back(std::make_pair(event, predecessor));
				}
			}
		}
	}

	std::cout << "\n\n";
	std::cout << "digraph {\n"
	          << "  rankdir=TB;\n";

	std::size_t event_id = 1;
	std::map<por::event::event const*, std::size_t> events;
	for(auto const& t : threads) {
		por::event::thread_id_t tid = t.first;
		if(tid == 0) {
			std::cout << "\n"
			          << "  subgraph cluster_T0 {\n"
			          << "    graph[style=invis]\n\n"
			          << "    node[shape=box style=dashed fixedsize=false width=1 label=\"\"]\n"
			          << "    // single visible node\n";

		} else {
			std::cout << "\n"
			          << "  subgraph cluster_T" << tid << " {\n"
			          << "    graph[label=\"Thread " << tid << "\"]\n\n"
			          << "    node[shape=box fixedsize=false width=1 label=\"\"]\n"
			          << "    // visible and invisible nodes\n";
		}

		// topological sort of thread's events
		auto const& relation = intra_thread_dependencies[tid];
		std::vector<por::event::event const*> thread_events;
		auto _init = std::find_if(threads[tid].begin(), threads[tid].end(), [](por::event::event const* e) {
			return e->kind() == por::event::event_kind::thread_init || e->kind() == por::event::event_kind::program_init;
		});
		assert(_init != threads[tid].end() && "each thread should have an init event");
		auto head = *_init;
		while(head != nullptr) {
			thread_events.push_back(head);
			auto it = std::find_if(relation.begin(), relation.end(), [&](auto const& edge) {
				return edge.second == head;
			});
			head = (it != relation.end()) ? it->first : nullptr;
		}

		std::size_t first_id = event_id;
		std::size_t depth = (*_init)->depth();
		std::vector<std::size_t> visibleNodes;
		for(auto* e : thread_events) {
			// account for difference in depth by inserting a number of invisible nodes
			for(std::size_t i = depth; (i + 1) < e->depth(); i++) {
				std::cout << "    e" << event_id++ << " [style=invis width=0 height=0]\n";
			}

			events[e] = event_id;
			visibleNodes.push_back(event_id);
			depth = e->depth();

			std::cout << "    e" << event_id << " [label=\"";
			switch(e->kind()) {
				case por::event::event_kind::program_init:
					std::cout << "init";
					break;
				case por::event::event_kind::local:
					std::cout << "loc";
					break;
				case por::event::event_kind::thread_init:
					std::cout << "+T";
					break;
				case por::event::event_kind::thread_join:
					std::cout << "join";
					break;
				case por::event::event_kind::thread_exit:
					std::cout << "-T";
					break;
				case por::event::event_kind::thread_create:
					std::cout << "create";
					break;
				case por::event::event_kind::lock_create:
					std::cout << "+L";
					break;
				case por::event::event_kind::lock_destroy:
					std::cout << "-L";
					break;
				case por::event::event_kind::lock_acquire:
					std::cout << "acq";
					break;
				case por::event::event_kind::lock_release:
					std::cout << "rel";
					break;
				case por::event::event_kind::condition_variable_create:
					std::cout << "+C";
					break;
				case por::event::event_kind::condition_variable_destroy:
					std::cout << "-C";
					break;
				case por::event::event_kind::wait1:
					std::cout << "w1";
					break;
				case por::event::event_kind::wait2:
					std::cout << "w2";
					break;
				case por::event::event_kind::signal:
					std::cout << "sig";
					break;
				case por::event::event_kind::broadcast:
					std::cout << "bro";
					break;
			}

			std::cout << " depth=" << e->depth();
			std::cout << "\"]\n";
			++event_id;
		}

		std::vector<std::pair<por::event::event const*, por::event::event const*>> deps = non_immediate_intra_thread_dependencies[tid];

		if(visibleNodes.size() > 1) {
			std::cout << "\n"
			          << "    edge[color=grey arrowtail=vee weight=1000 dir=back]\n"
			          << "    // visible edges\n";
			auto prev_it = visibleNodes.begin();
			for(auto it = visibleNodes.begin() + 1, ie = visibleNodes.end(); it != ie; ++it) {
				std::cout << "    e" << *prev_it << " -> e" << *it;
				por::event::event const* prev_event = thread_events[std::distance(visibleNodes.begin(), prev_it)];
				por::event::event const* curr_event = thread_events[std::distance(visibleNodes.begin(), it)];
				auto deps_it = std::find(deps.begin(), deps.end(), std::make_pair(curr_event, prev_event));
				if(deps_it != deps.end()) {
					std::cout << "[color=blue]";
					deps.erase(deps_it);
				}
				std::cout << "\n";
				prev_it = it;
			}
		}

		if((event_id - first_id) > visibleNodes.size()) {
			std::cout << "\n"
			          << "    edge[style=invis]\n"
			          << "    // invisible edges\n";
			auto prev_it = visibleNodes.begin();
			for(auto it = visibleNodes.begin() + 1, ie = visibleNodes.end(); it != ie; ++it) {
				bool gap = *it - *prev_it > 1;
				for(std::size_t i = *prev_it; gap && i <= (*it - 1); ++i) {
					std::cout << "    e" << i << " -> e" << i + 1 << "\n";
				}
				prev_it = it;
			}
		}

		if(deps.size() > 0) {
			std::cout << "\n"
			          << "    edge[color=blue arrowtail=vee style=dashed dir=back constraint=false weight=0]\n"
			          << "    // object dependency edges\n";
			for(auto &r : deps) {
				std::cout << "    e" << events[r.second] << " -> e" << events[r.first] << "\n";
			}
		}

		std::cout << "  }\n";
	}
	std::cout << "  edge[color=blue arrowtail=vee dir=back]\n"
	          << "  // dependency edges across threads\n";

	for(auto &r : inter_thread_dependencies) {
		std::cout << "  e" << events[r.second] << " -> e" << events[r.first] << "\n";
	}

	std::cout << "}\n";
}
