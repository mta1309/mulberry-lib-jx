/******************************************************************************
 JXImageMenu.cpp

	A menu that displays an image for each menu item.

	BASE CLASS = JXMenu

	Copyright � 1996 by John Lindal. All rights reserved.

 ******************************************************************************/

#include <JXStdInc.h>
#include <JXImageMenu.h>
#include <JXImageMenuData.h>
#include <JXImageMenuDirector.h>
#include <JXImage.h>
#include <jAssert.h>

/******************************************************************************
 Constructor

 ******************************************************************************/

JXImageMenu::JXImageMenu
	(
	const JCharacter*	title,
	const JSize			columnCount,
	JXContainer*		enclosure,
	const HSizingOption	hSizing,
	const VSizingOption	vSizing,
	const JCoordinate	x,
	const JCoordinate	y,
	const JCoordinate	w,
	const JCoordinate	h
	)
	:
	JXMenu(title, enclosure, hSizing, vSizing, x,y, w,h)
{
	JXImageMenuX(columnCount);
}

JXImageMenu::JXImageMenu
	(
	JXImage*			image,
	const JBoolean		menuOwnsImage,
	const JSize			columnCount,
	JXContainer*		enclosure,
	const HSizingOption	hSizing,
	const VSizingOption	vSizing,
	const JCoordinate	x,
	const JCoordinate	y,
	const JCoordinate	w,
	const JCoordinate	h
	)
	:
	JXMenu(image, menuOwnsImage, enclosure, hSizing, vSizing, x,y, w,h)
{
	JXImageMenuX(columnCount);
}

JXImageMenu::JXImageMenu
	(
	const JSize		columnCount,
	JXMenu*			owner,
	const JIndex	itemIndex,
	JXContainer*	enclosure
	)
	:
	JXMenu(owner, itemIndex, enclosure)
{
	JXImageMenuX(columnCount);
}

// private

void
JXImageMenu::JXImageMenuX
	(
	const JSize columnCount
	)
{
	itsIconMenuData = new JXImageMenuData(columnCount);
	assert( itsIconMenuData != NULL );

	SetBaseItemData(itsIconMenuData);
}

/******************************************************************************
 Destructor

 ******************************************************************************/

JXImageMenu::~JXImageMenu()
{
	ClearBaseItemData();
	delete itsIconMenuData;
}

/******************************************************************************
 New item

 ******************************************************************************/

void
JXImageMenu::InsertItem
	(
	const JIndex		index,
	JXImage*			image,
	const JBoolean		menuOwnsImage,
	const JBoolean		isCheckbox,
	const JBoolean		isRadio,
	const JCharacter*	id
	)
{
	itsIconMenuData->InsertItem(index, image, menuOwnsImage, isCheckbox, isRadio, id);
}

void
JXImageMenu::PrependItem
	(
	JXImage*			image,
	const JBoolean		menuOwnsImage,
	const JBoolean		isCheckbox,
	const JBoolean		isRadio,
	const JCharacter*	id
	)
{
	itsIconMenuData->PrependItem(image, menuOwnsImage, isCheckbox, isRadio, id);
}

void
JXImageMenu::AppendItem
	(
	JXImage*			image,
	const JBoolean		menuOwnsImage,
	const JBoolean		isCheckbox,
	const JBoolean		isRadio,
	const JCharacter*	id
	)
{
	itsIconMenuData->AppendItem(image, menuOwnsImage, isCheckbox, isRadio, id);
}

/******************************************************************************
 Item image

 ******************************************************************************/

const JXImage*
JXImageMenu::GetItemImage
	(
	const JIndex index
	)
	const
{
	return itsIconMenuData->GetImage(index);
}

void
JXImageMenu::SetItemImage
	(
	const JIndex	index,
	const JXPM&		data
	)
{
	JXImage* image = new JXImage(GetDisplay(), GetColormap(), data);
	assert( image != NULL );
	SetItemImage(index, image, kJTrue);
}

void
JXImageMenu::SetItemImage
	(
	const JIndex	index,
	JXImage*		image,
	const JBoolean	menuOwnsImage
	)
{
	itsIconMenuData->SetImage(index, image, menuOwnsImage);
}

/******************************************************************************
 CreateMenuWindow (virtual protected)

 ******************************************************************************/

JXMenuDirector*
JXImageMenu::CreateMenuWindow
	(
	JXWindowDirector* supervisor
	)
{
	JXImageMenuDirector* dir =
		new JXImageMenuDirector(supervisor, this, itsIconMenuData);
	assert( dir != NULL );
	return dir;
}

/******************************************************************************
 AdjustPopupChoiceTitle (virtual protected)

 ******************************************************************************/

void
JXImageMenu::AdjustPopupChoiceTitle
	(
	const JIndex index
	)
{
	const JXImage* titleImage = NULL;
	if (GetTitleImage(&titleImage))
		{
		JXImage* image = const_cast<JXImage*>(GetItemImage(index));
		SetTitle(NULL, image, kJFalse);
		}
}

/******************************************************************************
 Cast to JXImageMenu*

	Not inline because they are virtual

 ******************************************************************************/

JXImageMenu*
JXImageMenu::CastToJXImageMenu()
{
	return this;
}

const JXImageMenu*
JXImageMenu::CastToJXImageMenu()
	const
{
	return this;
}
