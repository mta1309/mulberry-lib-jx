/******************************************************************************
 JMMHashTable.cpp

	A JMMTable implemented with JHashTables.

	BASE CLASS = JMMTable

	Copyright � 1997 by Dustin Laurence.  All rights reserved.
	
	Base code generated by Codemill v0.1.0

 *****************************************************************************/

#include <JCoreStdInc.h>
#include <JMMRecord.h>

//Class Header
#include <JMMHashTable.h>
#include <jAssert.h>

	const JSize kInitialSize = 5000;

/******************************************************************************
 Constructor

 *****************************************************************************/

JMMHashTable::JMMHashTable
	(
	JMemoryManager* manager,
	const JBoolean  recordDelete
	)
	:
	JMMTable(manager),
	itsAllocatedTable(NULL),
	itsDeletedTable(NULL)
{
	itsAllocatedTable = new JHashTable<JMMRecord>(kInitialSize);
	assert(itsAllocatedTable != NULL);

	if (recordDelete)
		{
		itsDeletedTable = new JHashTable<JMMRecord>(kInitialSize);
		assert(itsDeletedTable != NULL);
		}
}

/******************************************************************************
 Destructor

 *****************************************************************************/

JMMHashTable::~JMMHashTable()
{
	delete itsAllocatedTable;
	itsAllocatedTable = NULL;

	delete itsDeletedTable;
	itsDeletedTable = NULL;
}

/******************************************************************************
 GetAllocatedCount (virtual)

 *****************************************************************************/

JSize
JMMHashTable::GetAllocatedCount() const
{
	return itsAllocatedTable->GetElementCount();
}

/******************************************************************************
 GetDeletedCount (virtual)

	Returns zero if the table is not recording deletions.

 *****************************************************************************/

JSize
JMMHashTable::GetDeletedCount() const
{
	if (itsDeletedTable != NULL)
		{
		return itsDeletedTable->GetElementCount();
		}
	else
		{
		return 0;
		}
}

/******************************************************************************
 GetTotalCount (virtual)

 *****************************************************************************/

JSize
JMMHashTable::GetTotalCount() const
{
	return GetAllocatedCount() + GetDeletedCount();
}

/******************************************************************************
 PrintAllocated (virtual)

 *****************************************************************************/

void
JMMHashTable::PrintAllocated
	(
	const JBoolean printInternal // = kJFalse
	)
	const
{
	cout << "\nAllocated block statistics:" << endl;

	cout << "\nAllocated user memory:" << endl;

	JConstHashCursor<JMMRecord> cursor(itsAllocatedTable);
	JSize totalSize = 0;
	while ( cursor.NextFull() )
		{
		JMMRecord thisRecord = cursor.GetValue();
		if ( !thisRecord.IsManagerMemory() )
			{
			PrintRecord(thisRecord);
			totalSize += thisRecord.GetSize();
			}
		}

	cout << "\nTotal allocated memory:  " << totalSize << " bytes" << endl;

	if (printInternal)
		{
		cout << "\nThe following blocks are probably owned by the memory manager"
		     << "\nand *should* still be allocated--please report all cases of user"
		     << "\nallocated memory showing up on this list!" << endl;

		cursor.Reset();
		while ( cursor.NextFull() )
			{
			JMMRecord thisRecord = cursor.GetValue();
			if ( thisRecord.IsManagerMemory() )
				{
				PrintRecord(thisRecord);
				}
			}
		}
}

/******************************************************************************
 _CancelRecordDeallocated (virtual)

 *****************************************************************************/

void
JMMHashTable::_CancelRecordDeallocated()
{
	if (itsDeletedTable != NULL)
		{
		delete itsDeletedTable;
		itsDeletedTable = NULL;
		}
}

/******************************************************************************
 _AddNewRecord (virtual protected)

 *****************************************************************************/

void
JMMHashTable::_AddNewRecord
	(
	const JMMRecord& record,
	const JBoolean   checkDoubleAllocation
	)
{
	JHashCursor<JMMRecord> cursor(itsAllocatedTable, reinterpret_cast<JHashValue>( record.GetAddress() ) );
	if (checkDoubleAllocation)
		{
		cursor.ForceNextMapInsertHash();
		if ( cursor.IsFull() )
			{
			JMMRecord thisRecord = cursor.GetValue();
			NotifyMultipleAllocation(record, thisRecord);
			}
		// Might as well trust malloc--the table should never have duplicate
		// entries!
		}
	else
		{
		cursor.ForceNextOpen();
		}
	cursor.Set(reinterpret_cast<JHashValue>( record.GetAddress() ), record);
}

/******************************************************************************
 _SetRecordDeleted

 *****************************************************************************/

JBoolean
JMMHashTable::_SetRecordDeleted
	(
	JMMRecord*        record,
	const void*       block,
	const JCharacter* file,
	const JUInt32     line,
	const JBoolean    isArray
	)
{
	JHashCursor<JMMRecord> allocCursor(itsAllocatedTable, reinterpret_cast<JHashValue>(block) );
	if ( allocCursor.NextHash() )
		{
		JMMRecord thisRecord = allocCursor.GetValue();
		thisRecord.SetDeleteLocation(file, line, isArray);

		if (!thisRecord.ArrayNew() && isArray)
			{
			NotifyObjectDeletedAsArray(thisRecord);
			}
		else if (thisRecord.ArrayNew() && !isArray)
			{
			NotifyArrayDeletedAsObject(thisRecord);
			}

		allocCursor.Remove();
		if (itsDeletedTable != NULL)
			{
			JHashCursor<JMMRecord> deallocCursor(itsDeletedTable, reinterpret_cast<JHashValue>(block) );
			deallocCursor.ForceNextOpen();
			deallocCursor.Set(reinterpret_cast<JHashValue>(block), thisRecord);
			}

		*record = thisRecord;
		return kJTrue;
		}
	else
		{
		if (itsDeletedTable == NULL)
			{
			NotifyUnallocatedDeletion(file, line, isArray);
			}
		else
			{
			JHashCursor<JMMRecord> deallocCursor(itsDeletedTable, reinterpret_cast<JHashValue>(block) );
			if ( deallocCursor.NextHash() )
				{
				// Seek most recent deallocation at that address
				JMMRecord previousRecord = deallocCursor.GetValue();
				while ( deallocCursor.NextHash() )
					{
					JMMRecord thisRecord = deallocCursor.GetValue();
					if ( thisRecord.GetID() > previousRecord.GetID() )
						{
						previousRecord = thisRecord;
						}
					}

				NotifyMultipleDeletion(previousRecord, file, line, isArray);
				}
			else
				{
				NotifyUnallocatedDeletion(file, line, isArray);
				}
			}
		return kJFalse;
		}
}