#include "pseudoalloc/pseudoalloc.h"

#if defined(USE_GTEST_INSTEAD_OF_MAIN)
	#include "gtest/gtest.h"
#endif

#include <cassert>
#include <deque>
#include <iomanip>
#include <iostream>

#if defined(USE_GTEST_INSTEAD_OF_MAIN)
int allocate_sample_test() {
#else
int main() {
#endif
	// initialize a mapping and an associated allocator (using the location "0" gives an OS-assigned location)
	pseudoalloc::mapping_t mapping(static_cast<std::size_t>(1) << 12);
	pseudoalloc::allocator_t allocator(mapping, 0); /// allocator without a quarantine zone

	std::deque<void*> allocations;
	for(std::size_t i = 0; i < 10; ++i) {
		auto p = allocator.allocate(4);
		allocations.emplace_back(p);
		std::cout << "Allocated     " << std::right << std::setw(64 / 4) << std::hex
		          << (static_cast<char*>(p) - static_cast<char*>(mapping.begin())) << "\n";
	}

	{
		auto p = allocations[2];
		allocations.erase(allocations.begin() + 2);
		std::cout << "Freeing       " << std::right << std::setw(64 / 4) << std::hex
		          << (static_cast<char*>(p) - static_cast<char*>(mapping.begin())) << "\n";
		allocator.free(p, 4);
	}

	for(auto p : allocations) {
		std::cout << "Freeing       " << std::right << std::setw(64 / 4) << std::hex
		          << (static_cast<char*>(p) - static_cast<char*>(mapping.begin())) << "\n";
		allocator.free(p, 4);
	}

	exit(0);
}

#if defined(USE_GTEST_INSTEAD_OF_MAIN)
TEST(PseudoallocDeathTest, AllocateSample) { ASSERT_EXIT(allocate_sample_test(), ::testing::ExitedWithCode(0), ""); }
#endif
