/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __DATAARRAY_H__
#define __DATAARRAY_H__

#include <iterator>
#include <memory>
#include <new>
#include "PvzpDebug.h"
#include "PvzpCommon.h"

enum
{
	DATA_ARRAY_INDEX_MASK = 65535,
	DATA_ARRAY_KEY_MASK = -65536,
	DATA_ARRAY_KEY_SHIFT = 16,
	DATA_ARRAY_MAX_SIZE = 65536,
	DATA_ARRAY_KEY_FIRST = 1
};

template <typename T> class DataArray
{
	// Value-initialization zeroes T's members only while this constructor stays implicit.
	struct DataArrayItem : T {};

	std::unique_ptr<DataArrayItem[]>	mItems;
	std::unique_ptr<unsigned int[]>		mItemIds;

public:
	unsigned int			mMaxUsedCount = 0U;
	unsigned int			mMaxSize = 0U;
	unsigned int			mFreeListHead = 0U;
	unsigned int			mSize = 0U;
	unsigned int			mNextKey = 1U;
	const char*				mName = nullptr;

public:
	DataArray() = default;

	~DataArray()
	{
		DataArrayDispose();
	}

	void DataArrayInitialize(unsigned int theMaxSize, const char* theName)
	{
		PVZP_ASSERT(mItems == nullptr);
		// Commit only after both allocations succeed; a larger-pool retry stays safe.
		auto items = std::make_unique<DataArrayItem[]>(theMaxSize);
		auto ids = std::make_unique<unsigned int[]>(theMaxSize);
		mItems = std::move(items);
		mItemIds = std::move(ids);
		mMaxSize = theMaxSize;
		mNextKey = 1001U;
		mName = theName;
	}

	void DataArrayDispose()
	{
		DataArrayFreeAll();  // no-ops when uninitialized
		mItems.reset();
		mItemIds.reset();
		mMaxUsedCount = 0U;
		mMaxSize = 0U;
		mFreeListHead = 0U;
		mSize = 0U;
		mName = nullptr;
	}

	void DataArrayFree(T* theItem)
	{
		unsigned int anIndex = static_cast<unsigned int>(static_cast<DataArrayItem*>(theItem) - mItems.get());
		PVZP_ASSERT(DataArrayGet(mItemIds[anIndex]) == theItem, "Failed: DataArrayFree({:p}) in {}", (void*)theItem, mName);
		DataArrayResetItemAt(anIndex);
		mItemIds[anIndex] = mFreeListHead;
		mFreeListHead = anIndex;
		mSize--;
	}

	void DataArrayFreeAll()
	{
		for (T* aItem : *this)
			DataArrayFree(aItem);

		mFreeListHead = 0U;
		mMaxUsedCount = 0U;
	}

	inline unsigned int DataArrayGetID(T* theItem)
	{
		unsigned int anIndex = static_cast<unsigned int>(static_cast<DataArrayItem*>(theItem) - mItems.get());
		unsigned int anId = mItemIds[anIndex];
		PVZP_ASSERT(DataArrayGet(anId) == theItem, "Failed: DataArrayGetID({:p}) for {}", (void*)theItem, mName);
		return anId;
	}

	// Index-ascending, live-slots-only iteration; end re-checked against the live mMaxUsedCount.
	// The item address is resolved on dereference, so freeing it mid-loop is safe.
	class Iterator
	{
		const DataArray*	mArray;
		unsigned int		mIndex;

	public:
		Iterator(const DataArray* theArray, unsigned int theIndex) : mArray(theArray), mIndex(theIndex) {}

		T* operator*() const
		{
			return &mArray->mItems[mIndex];
		}

		Iterator& operator++()
		{
			do
			{
				mIndex++;
			} while (mIndex < mArray->mMaxUsedCount && !(mArray->mItemIds[mIndex] & DATA_ARRAY_KEY_MASK));
			return *this;
		}

		bool operator==(std::default_sentinel_t) const
		{
			return mIndex >= mArray->mMaxUsedCount;
		}
	};

	Iterator begin() const
	{
		Iterator anIterator(this, 0U);
		if (mMaxUsedCount != 0U && !(mItemIds[0] & DATA_ARRAY_KEY_MASK))
			++anIterator;
		return anIterator;
	}

	std::default_sentinel_t end() const
	{
		return std::default_sentinel;
	}

	// PC effect code uses a cursor; keep the same ascending live-slot order as range iteration.
	bool DataArrayIterate(T*& theItem)
	{
		unsigned int index = theItem ? static_cast<unsigned int>(static_cast<DataArrayItem*>(theItem) - mItems.get()) + 1U : 0U;
		while (index < mMaxUsedCount && !(mItemIds[index] & DATA_ARRAY_KEY_MASK)) ++index;
		theItem = index < mMaxUsedCount ? &mItems[index] : nullptr;
		return theItem != nullptr;
	}

	// Checked allocation is opt-in until every legacy caller handles failure.
	bool DataArrayCanAllocate(unsigned int theCount = 1) const
	{
		if (mMaxUsedCount > mMaxSize || mSize > mMaxUsedCount || mFreeListHead > mMaxUsedCount)
			return false;
		if ((mFreeListHead == mMaxUsedCount) != (mSize == mMaxUsedCount)) return false;
		return theCount <= mMaxSize - mSize;
	}

	T* DataArrayTryAlloc()
	{
		if (!DataArrayCanAllocate()) return nullptr;
		if (!mItems || !mItemIds) return nullptr;
		if (mFreeListHead < mMaxUsedCount &&
			(mItemIds[mFreeListHead] > mMaxUsedCount || mItemIds[mFreeListHead] == mFreeListHead)) return nullptr;
		return DataArrayAlloc();
	}

	// Validate loaded IDs/free links before callers can iterate or allocate.
	bool DataArrayValidateState() const
	{
		if (!DataArrayCanAllocate(0) || mNextKey == 0 || mNextKey >= DATA_ARRAY_MAX_SIZE) return false;
		unsigned int aLive = 0;
		for (unsigned int i = 0; i < mMaxUsedCount; ++i)
		{
			const unsigned int aId = mItemIds[i];
			if (aId & DATA_ARRAY_KEY_MASK)
			{
				if ((aId & DATA_ARRAY_INDEX_MASK) != i) return false;
				++aLive;
			}
			else if (aId > mMaxUsedCount) return false;
		}
		if (aLive != mSize) return false;
		unsigned int aNext = mFreeListHead;
		for (unsigned int n = 0; n < mMaxUsedCount - mSize; ++n)
		{
			if (aNext >= mMaxUsedCount) return false;
			aNext = mItemIds[aNext];
		}
		return aNext == mMaxUsedCount;
	}

	T* DataArrayAlloc()
	{
		PVZP_ASSERT(mSize < mMaxSize, "Data array full: {}", mName);
		PVZP_ASSERT(mFreeListHead <= mMaxUsedCount, "DataArrayAlloc error in {}", mName);
		unsigned int aNext = mMaxUsedCount;
		if (mFreeListHead == mMaxUsedCount)
			mFreeListHead = ++mMaxUsedCount;
		else
		{
			aNext = mFreeListHead;
			mFreeListHead = mItemIds[mFreeListHead];
		}

		T& aNewItem = DataArrayResetItemAt(aNext);
		mItemIds[aNext] = (mNextKey++ << DATA_ARRAY_KEY_SHIFT) | aNext;
		if (mNextKey == DATA_ARRAY_MAX_SIZE) mNextKey = 1;
		mSize++;

		return &aNewItem;
	}

	T* DataArrayTryToGet(unsigned int theId)
	{
		if (!theId || !(theId & DATA_ARRAY_KEY_MASK) || (theId & DATA_ARRAY_INDEX_MASK) >= mMaxUsedCount)
			return nullptr;

		unsigned int anIndex = theId & DATA_ARRAY_INDEX_MASK;
		return (mItemIds[anIndex] == theId) ? &mItems[anIndex] : nullptr;
	}

	T* DataArrayGet(unsigned int theId)
	{
		PVZP_ASSERT(DataArrayTryToGet(theId) != nullptr, "Failed: DataArrayGet(0x{:x}) for {}", theId, mName);
		return &mItems[theId & DATA_ARRAY_INDEX_MASK];
	}

	T& DataArrayGetItemAt(unsigned int theIndex)
	{
		return mItems[theIndex];
	}

	T& DataArrayResetItemAt(unsigned int theIndex)
	{
		DataArrayItem* aItem = &mItems[theIndex];
		std::destroy_at(aItem);
		return *std::construct_at(aItem);
	}

	unsigned int& DataArrayGetIDAt(unsigned int theIndex)
	{
		return mItemIds[theIndex];
	}
};

#endif
