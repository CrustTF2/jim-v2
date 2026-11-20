#ifndef ALLOCATION_MANAGER_HPP
#define ALLOCATION_MANAGER_HPP

#include "base/jutil_base.hpp"
#include <mutex>

namespace JUTIL
{

	/*
	 * Allocation list data structure.
	 */
	struct Allocation
	{
		// Allocation information.
		size_t num;
		void* address;
		size_t size;

		// List structure.
		struct Allocation* prev;
		struct Allocation* next;
	};

	/*
	 * Class for tracking allocations, reallocations, and frees on heap.
	 */
	class AllocationManager
	{

	public:

		// Private constructor and destructor.
		AllocationManager( void );
		~AllocationManager( void );

		// Allocation debugging.
		void JUTIL_API set_debug_break( size_t allocation_num );
		void JUTIL_API dump_leaks( const char* filename );

		// Allocation tracking.
		void JUTIL_API on_allocate( void* address, size_t size );
		void JUTIL_API on_reallocate( void* address, void* new_address, size_t new_size );
		void JUTIL_API on_release( void* address );

	public:

		// Singleton instance creation.
		static JUTIL_API AllocationManager* get_instance( void );
		static JUTIL_API void shut_down( void );

	private:

		// Find structure for existing allocations.
		struct Allocation* find_allocation( void* address );

	private:

		static AllocationManager* instance_;

	private:

		struct Allocation* root_;
		size_t allocation_num_;
		size_t break_allocation_;

		// Allocation logging mutex.
		std::mutex allocation_mutex_;

	};

}

#endif // ALLOCATION_MANAGER_HPP
