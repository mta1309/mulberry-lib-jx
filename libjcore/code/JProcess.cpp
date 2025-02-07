/******************************************************************************
 JProcess.cpp

	Class to represent a UNIX process.

	Event loops can call CheckForFinishedChild(kJFalse) in order to
	receive JBroadcaster messages when child processes finish.

	BASE CLASS = virtual JBroadcaster

	Copyright � 1997 by John Lindal. All rights reserved.

 *****************************************************************************/

#include <JCoreStdInc.h>
#include <JThisProcess.h>
#include <jAssert.h>

// JBroadcaster message types

const JCharacter* JProcess::kFinished = "Finished::JProcess";

// static data

JPtrArray<JProcess>	JProcess::theProcessList(JPtrArrayT::kForgetAll);

/******************************************************************************
 Create (static)

	Refer to JExecute() for documentation.

 ******************************************************************************/

JError
JProcess::Create
	(
	JProcess**				process,
	const JCharacter*		cmdStr,
	const JExecuteAction	toAction,
	int*					toFD,
	const JExecuteAction	fromAction,
	int*					fromFD,
	const JExecuteAction	errAction,
	int*					errFD
	)
{
	pid_t childPID;
	const JError err = JExecute(cmdStr, &childPID, toAction, toFD,
								fromAction, fromFD, errAction, errFD);
	if (err.OK())
		{
		*process = new JProcess(childPID);
		assert( *process != NULL );
		}
	else
		{
		*process = NULL;
		}

	return err;
}

JError
JProcess::Create
	(
	JProcess**					process,
	const JPtrArray<JString>&	argList,
	const JExecuteAction		toAction,
	int*						toFD,
	const JExecuteAction		fromAction,
	int*						fromFD,
	const JExecuteAction		errAction,
	int*						errFD
	)
{
	pid_t childPID;
	const JError err = JExecute(argList, &childPID, toAction, toFD,
								fromAction, fromFD, errAction, errFD);
	if (err.OK())
		{
		*process = new JProcess(childPID);
		assert( *process != NULL );
		}
	else
		{
		*process = NULL;
		}

	return err;
}

JError
JProcess::Create
	(
	JProcess**				process,
	const JCharacter*		argv[],
	const JSize				size,
	const JExecuteAction	toAction,
	int*					toFD,
	const JExecuteAction	fromAction,
	int*					fromFD,
	const JExecuteAction	errAction,
	int*					errFD
	)
{
	pid_t childPID;
	const JError err = JExecute(argv, size, &childPID, toAction, toFD,
								fromAction, fromFD, errAction, errFD);
	if (err.OK())
		{
		*process = new JProcess(childPID);
		assert( *process != NULL );
		}
	else
		{
		*process = NULL;
		}

	return err;
}

/******************************************************************************
 Constructor

 ******************************************************************************/

JProcess::JProcess
	(
	const pid_t pid
	)
{
	itsPID            = pid;
	itsIsFinishedFlag = kJFalse;
	itsFinishedStatus = 0;
	itsAutoDeleteFlag = kJFalse;

	theProcessList.SetCompareFunction(ComparePID);
	theProcessList.InsertSorted(this, kJTrue);

	JThisProcess::QuitAtExit(this, kJTrue);
}

// private -- used only by CheckForFinishedChild()
//			  to avoid adding temporary object to theProcessList

JProcess::JProcess
	(
	const pid_t	pid,
	const int	x
	)
{
	itsPID = pid;
}

/******************************************************************************
 Destructor

 ******************************************************************************/

JProcess::~JProcess()
{
	theProcessList.Remove(this);
	JThisProcess::QuitAtExit(this, kJFalse);
	JThisProcess::KillAtExit(this, kJFalse);
}

/******************************************************************************
 Quit/Kill at exit

 ******************************************************************************/

JBoolean
JProcess::WillQuitAtExit()
	const
{
	return JThisProcess::WillQuitAtExit(this);
}

void
JProcess::QuitAtExit
	(
	const JBoolean quit
	)
{
	JThisProcess::QuitAtExit(this, quit);
}

JBoolean
JProcess::WillKillAtExit()
	const
{
	return JThisProcess::WillKillAtExit(this);
}

void
JProcess::KillAtExit
	(
	const JBoolean kill
	)
{
	JThisProcess::KillAtExit(this, kill);
}

/******************************************************************************
 SendSignalToGroup

 ******************************************************************************/

JError
JProcess::SendSignalToGroup
	(
	const int signal
	)
{
	pid_t pgid;
	const JError err = GetPGID(&pgid);
	if (err.OK())
		{
		return JSendSignalToGroup(pgid, signal);
		}
	else
		{
		return err;
		}
}

/******************************************************************************
 WaitUntilFinished

	Block until the child finishes.  This does not broadcast.

 ******************************************************************************/

void
JProcess::WaitUntilFinished()
{
	ACE_exitcode status;
	const JError err = JWaitForChild(itsPID, &status);
	if (err.OK())
		{
		itsIsFinishedFlag = kJTrue;
		itsFinishedStatus = status;
		}
}

/******************************************************************************
 CheckForFinishedChild (static)

	Checks if a child has finished.  If one has, an a JProcess object
	exists for it, then tell it to Broadcast().

 ******************************************************************************/

void
JProcess::CheckForFinishedChild
	(
	const JBoolean block
	)
{
	pid_t pid;
	ACE_exitcode status;
	const JError err = JWaitForChild(block, &pid, &status);
	if (err.OK() && pid > 0)
		{
		theProcessList.SetCompareFunction(ComparePID);

		JProcess target(pid, 0);
		JIndex i;
		if (theProcessList.SearchSorted(&target, JOrderedSetT::kFirstMatch, &i))
			{
			// Broadcast message from each JProcess object with the
			// given pid.  There could be more than one!

			// Since theProcessList may be changed by code in some Receive(),
			// we first collect all the objects that need to broadcast,
			// and then we tell each one to broadcast.

			JPtrArray<JProcess> list(JPtrArrayT::kForgetAll);
			const JSize processCount = theProcessList.GetElementCount();
			while (i <= processCount)
				{
				JProcess* p = theProcessList.NthElement(i);
				if (p->GetPID() != pid)
					{
					break;
					}

				list.Append(p);
				i++;
				}

			const JSize bcastCount = list.GetElementCount();
			for (i=1; i<=bcastCount; i++)
				{
				JProcess* p = list.NthElement(i);
				const JBoolean autoDelete = p->itsAutoDeleteFlag;	// save since Broadcast() might delete it
				p->itsIsFinishedFlag = kJTrue;
				p->itsFinishedStatus = status;
				p->Broadcast(Finished(status));
				if (autoDelete)
					{
					delete p;
					}
				}
			}
		}
}

/******************************************************************************
 ComparePID (static private)

 ******************************************************************************/

JOrderedSetT::CompareResult
JProcess::ComparePID
	(
	JProcess* const & p1,
	JProcess* const & p2
	)
{
	if (p1->GetPID() > p2->GetPID())
		{
		return JOrderedSetT::kFirstGreaterSecond;
		}
	else if (p1->GetPID() < p2->GetPID())
		{
		return JOrderedSetT::kFirstLessSecond;
		}
	else
		{
		return JOrderedSetT::kFirstEqualSecond;
		}
}

#define JTemplateType JProcess
#include <JPtrArray.tmpls>
#undef JTemplateType
