/***************************************************************************
      addresstemplateprovider - template provider class for addresses
                             -------------------
    begin                : Jun 2007
    copyright            : (C) 2007 by Klaas Freitag
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

#include <kdebug.h>
#include <klocale.h>
#include <krun.h>

#include "addresstemplateprovider.h"
#include "texteditdialog.h"
#include "doctext.h"
#include "defaultprovider.h"

AddressTemplateProvider::AddressTemplateProvider( QWidget *parent )
  :TemplateProvider( parent )
{

}

void AddressTemplateProvider::slotNewTemplate()
{
  kdDebug() << "SlotNewTemplate for addresses called!" << endl;

  KRun::runCommand( QString::fromLatin1( "kaddressbook --new-contact" ),
                    QString::fromLatin1("kaddressbook" ), "address" );
}

void AddressTemplateProvider::slotEditTemplate()
{
  kdDebug() << "SlotEditTemplate called!" << endl;
}

void AddressTemplateProvider::slotDeleteTemplate()
{

}

void AddressTemplateProvider::slotSetCurrentAddress( const Addressee& adr )
{
  mCurrentAddress = adr;
}

void AddressTemplateProvider::slotTemplateToDocument()
{
  kdDebug() << "Moving template to document" << endl;

  emit addressToDocument( mCurrentAddress );
}

#include "addresstemplateprovider.moc"
