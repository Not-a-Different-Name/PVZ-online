#pragma once
#include <cstdint>
#include "../../PvzpLib/DataArray.h"

// Validate counts in locals before exposing them to iteration or allocation.
template <typename T, typename TContext>
static bool SyncPoolMetadata(TContext& theContext, DataArray<T>& theDataArray)
{
	if (theContext.mFailed) return false;
	uint32_t aFree = theDataArray.mFreeListHead, aUsed = theDataArray.mMaxUsedCount;
	uint32_t aSize = theDataArray.mSize, aKey = theDataArray.mNextKey, aCapacity = theDataArray.mMaxSize;
	theContext.SyncUInt32(aFree);
	theContext.SyncUInt32(aUsed);
	theContext.SyncUInt32(aSize);
	theContext.SyncUInt32(aKey);
	theContext.SyncUInt32(aCapacity);
	// Accept old smaller capacities after expansion; reject oversized or inconsistent saves.
	if (theContext.mFailed || aCapacity == 0 || aCapacity > theDataArray.mMaxSize ||
		aUsed > aCapacity || aSize > aUsed || aFree > aUsed || aKey == 0 || aKey >= DATA_ARRAY_MAX_SIZE)
	{ theContext.mFailed = true; return false; }
	if (theContext.mReading)
	{
		theDataArray.mFreeListHead = aFree;
		theDataArray.mMaxUsedCount = aUsed;
		theDataArray.mSize = aSize;
		theDataArray.mNextKey = aKey;
	}
	return true;
}

template <typename T, typename TContext>
static void FinishPoolRead(TContext& theContext, DataArray<T>& theDataArray)
{
	if (theContext.mFailed || !theDataArray.DataArrayValidateState())
	{
		theContext.mFailed = true;
		if (theContext.mReading) theDataArray.mFreeListHead = theDataArray.mMaxUsedCount = theDataArray.mSize = 0;
	}
}

template <typename TContext>
static bool CanReadPortableItemData(TContext& theContext, uint32_t theLength)
{
	if (theContext.mFailed || theLength > theContext.mReader->RemainingMemoryBytes())
	{ theContext.mFailed = true; return false; }
	return true;
}
