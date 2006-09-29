/***************************************************************************
 inserttemplatedialog.h  - small dialog to insert templates into documents
                             -------------------
    begin                : Sep 2006
    copyright            : (C) 2006 Klaas Freitag
    email                : freitag@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef INSERTTEMPLDIALOG_H
#define INSERTTEMPLDIALOG_H

#include <kdialogbase.h>

#include "docposition.h"

class insertTmplBase;

class InsertTemplDialog: public KDialogBase
{
    Q_OBJECT

public:
    InsertTemplDialog( QWidget* );
  ~InsertTemplDialog();
  void setDocPosition( DocPosition * );
  void setPositionList( DocPositionList, int );
  DocPosition docPosition();

private:
  insertTmplBase *mBaseWidget;
  DocPosition mParkPosition;
};

#endif