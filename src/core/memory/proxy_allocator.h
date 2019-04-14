/*
 * Copyright (c) 2012-2018 Daniele Bartolini and individual contributors.
 * License: https://github.com/dbartolini/crown/blob/master/LICENSE
 */

#pragma once

#include "core/memory/allocator.h"

namespace crown
{
/// Offers the facility to tag allocators by a string identifier.
///
/// @ingroup Memory
struct ProxyAllocator : public Allocator
{
	Allocator& _allocator;
	const char* _name;

	/// Tag all allocations made with @a allocator by the given @a name
	ProxyAllocator(Allocator& allocator, const char* name);

	/// @copydoc Allocator::allocate()
	void* allocate(u32 size, u32 align = Allocator::DEFAULT_ALIGN);

	/// @copydoc Allocator::deallocate()
	void deallocate(void* data);

	/// @copydoc Allocator::allocated_size()
	u32 allocated_size(const void* /*ptr*/) { return SIZE_NOT_TRACKED; }

	/// @copydoc Allocator::total_allocated()
	u32 total_allocated() { return SIZE_NOT_TRACKED; }

	/// Returns the name of the proxy allocator
	const char* name() const;
};

} // namespace crown
