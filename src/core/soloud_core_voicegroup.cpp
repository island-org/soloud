/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#include "soloud.h"

// Voice group operations

namespace SoLoud
{
	// Create a voice group. Returns 0 if unable (out of voice groups / out of memory)
	handle Soloud::createVoiceGroup()
	{
		if (mLockMutexFunc) mLockMutexFunc(mMutex);

		int i;
		// Check if there's any deleted voice groups and re-use if found
		for (i = 0; i < mVoiceGroupCount; i++)
		{
			if (mVoiceGroup[i] == NULL)
			{
				mVoiceGroup[i] = new unsigned int[16];
				if (mVoiceGroup[i] == NULL)
				{
					if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
					return 0;
				}
				mVoiceGroup[i][0] = 16;
				mVoiceGroup[i][1] = 0;
				if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
				return 0xfffff000 | i;
			}		
		}
		if (mVoiceGroupCount == 4096)
		{
			if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
			return 0;
		}
		int oldcount = mVoiceGroupCount;
		if (mVoiceGroupCount == 0)
		{
			mVoiceGroupCount = 4;
		}
		mVoiceGroupCount *= 2;
		unsigned int **vg = new unsigned int * [mVoiceGroupCount];
		if (vg == NULL)
		{
			mVoiceGroupCount = oldcount;
			if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
			return 0;
		}
		for (i = 0; i < oldcount; i++)
		{
			vg[i] = mVoiceGroup[i];
		}

		for (; i < mVoiceGroupCount; i++)
		{
			vg[i] = NULL;
		}

		delete[] mVoiceGroup;
		mVoiceGroup = vg;
		i = oldcount;
		mVoiceGroup[i] = new unsigned int[16];
		if (mVoiceGroup[i] == NULL)
		{
			if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
			return 0;
		}
		mVoiceGroup[i][0] = 16;
		mVoiceGroup[i][1] = 0;
		if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
		return 0xfffff000 | i;
	}

	// Destroy a voice group. 
	result Soloud::destroyVoiceGroup(handle aVoiceGroupHandle)
	{
		if (!isVoiceGroup(aVoiceGroupHandle))
			return INVALID_PARAMETER;
		int c = aVoiceGroupHandle & 0xfff;
		
		if (mLockMutexFunc) mLockMutexFunc(mMutex);
		delete[] mVoiceGroup[c];
		mVoiceGroup[c] = NULL;
		if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
		return SO_NO_ERROR;
	}

	// Add a voice handle to a voice group
	result Soloud::addVoiceToGroup(handle aVoiceGroupHandle, handle aVoiceHandle)
	{
		if (!isVoiceGroup(aVoiceGroupHandle))
			return INVALID_PARAMETER;
		
		// Don't consider adding invalid voice handles as an error, since the voice may just have ended.
		if (!isValidVoiceHandle(aVoiceHandle))
			return SO_NO_ERROR;

		trimVoiceGroup(aVoiceGroupHandle);
		
		int c = aVoiceGroupHandle & 0xfff;
		unsigned int i;

		if (mLockMutexFunc) mLockMutexFunc(mMutex);

		for (i = 1; i < mVoiceGroup[c][0]; i++)
		{
			if (mVoiceGroup[c][i] == aVoiceHandle)
			{
				if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
				return SO_NO_ERROR; // already there
			}

			if (mVoiceGroup[c][i] == 0)
			{
				mVoiceGroup[c][i] = aVoiceHandle;
				mVoiceGroup[c][i + 1] = 0;
				
				if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
				return SO_NO_ERROR;
			}
		}
		
		// Full group, allocate more memory
		unsigned int * n = new unsigned int[mVoiceGroup[c][0] * 2 + 1];
		if (n == NULL)
		{
			if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
			return OUT_OF_MEMORY;
		}
		for (i = 0; i < mVoiceGroup[c][0]; i++)
			n[i] = mVoiceGroup[c][i];
		n[n[0]] = aVoiceHandle;
		n[n[0]+1] = 0;
		n[0] *= 2;
		delete[] mVoiceGroup[c];
		mVoiceGroup[c] = n;
		if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
		return SO_NO_ERROR;
	}

	// Is this handle a valid voice group?
	bool Soloud::isVoiceGroup(handle aVoiceGroupHandle)
	{
		if ((aVoiceGroupHandle & 0xfffff000) != 0xfffff000)
			return 0;
		int c = aVoiceGroupHandle & 0xfff;
		if (c >= mVoiceGroupCount)
			return 0;

		if (mLockMutexFunc) mLockMutexFunc(mMutex);		
		bool res = mVoiceGroup[c] == NULL;		
		if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);

		return res;
	}

	// Is this voice group empty?
	bool Soloud::isVoiceGroupEmpty(handle aVoiceGroupHandle)
	{
		// If not a voice group, yeah, we're empty alright..
		if (!isVoiceGroup(aVoiceGroupHandle))
			return 1;
		trimVoiceGroup(aVoiceGroupHandle);
		int c = aVoiceGroupHandle & 0xfff;

		if (mLockMutexFunc) mLockMutexFunc(mMutex);
		bool res = mVoiceGroup[c][1] == 0;
		if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);

		return res;
	}

	// Remove all non-active voices from group
	void Soloud::trimVoiceGroup(handle aVoiceGroupHandle)
	{
		if (!isVoiceGroup(aVoiceGroupHandle))
			return;
		int c = aVoiceGroupHandle & 0xfff;

		if (mLockMutexFunc) mLockMutexFunc(mMutex);
		// empty group
		if (mVoiceGroup[c][1] == 0)
		{
			if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
			return;
		}

		unsigned int i;
		for (i = 1; i < mVoiceGroup[c][0]; i++)
		{
			if (mVoiceGroup[c][i] == 0)
			{
				if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
				return;
			}
			
			if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
			while (!isValidVoiceHandle(mVoiceGroup[c][i])) // function locks mutex, so we need to unlock it before the call
			{
				if (mLockMutexFunc) mLockMutexFunc(mMutex);
				unsigned int j;
				for (j = i; j < mVoiceGroup[c][0] - 1; j++)
				{
					mVoiceGroup[c][j] = mVoiceGroup[c][j + 1];
					if (mVoiceGroup[c][j] == 0)
						break;
				}
				mVoiceGroup[c][mVoiceGroup[c][0] - 1] = 0;				
				if (mVoiceGroup[c][i] == 0)
				{
					if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
					return;
				}
			}
		}
		if (mUnlockMutexFunc) mUnlockMutexFunc(mMutex);
	}

	handle *Soloud::voiceGroupHandleToArray(handle aVoiceGroupHandle)
	{
		if ((aVoiceGroupHandle & 0xfffff000) != 0xfffff000)
			return NULL;
		int c = aVoiceGroupHandle & 0xfff;
		if (c >= mVoiceGroupCount)
			return NULL;
		if (mVoiceGroup[c] == NULL)
			return NULL;
		return mVoiceGroup[c] + 1;
	}

}