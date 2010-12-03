/***************************************************************************
                          templkatalogview.cpp
                             -------------------
    begin                : 2005-07-09
    copyright            : (C) 2005 by Klaas Freitag
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

// include files for QT
#include <QDir>
#include <QPrinter>
#include <QPainter>
#include <QLayout>

// include files for KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstandardaction.h>
#include <kdebug.h>

// application specific includes
#include "katalogview.h"
#include "templkatalogview.h"
#include "floskeltemplate.h"
#include "kataloglistview.h"
#include "flostempldialog.h"
#include "templkatalog.h"
#include "templkataloglistview.h"
#include "katalogman.h"
#include "filterheader.h"
#include "documentman.h"
#
#define ID_STATUS_MSG 1

TemplKatalogView::TemplKatalogView(QWidget* parent, const char* name)
    : KatalogView(parent, name),
      m_flosDialog(0),
      m_listview(0)
{

}

TemplKatalogView::~TemplKatalogView()
{
  delete m_flosDialog;
}

Katalog* TemplKatalogView::getKatalog( const QString& name )
{
    Katalog *k = KatalogMan::self()->getKatalog( name );
    if( ! k ) {
        k = new TemplKatalog( name );
        KatalogMan::self()->registerKatalog( k );
    }
    return k;
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////


void TemplKatalogView::slEditTemplate()
{
    TemplKatalogListView* listview = static_cast<TemplKatalogListView*>(getListView());

    if( listview )
    {
        FloskelTemplate *currTempl = static_cast<FloskelTemplate*> (listview->currentItemData());
        if( currTempl ) {
            QTreeWidgetItem *item = (QTreeWidgetItem*) listview->currentItem();
            openDialog( item, currTempl, false );
        }
    }
}

void TemplKatalogView::slNewTemplate()
{
    KatalogListView *listview = getListView();
    if( !listview ) return;
    TemplKatalogListView *templListView = static_cast<TemplKatalogListView*>(listview);

    // Anlegen eines neuen Templates
    FloskelTemplate *flosTempl = new FloskelTemplate();
    flosTempl->setText( i18n( "<new template>" ) );

    // Eltern = Katalogitem rausfinden
    QTreeWidgetItem *parentItem = static_cast<QTreeWidgetItem*>(listview->currentItem());
    if( parentItem )
    {
        // Wenn es kein chapter ist, nehmen wir den Parent
        if( ! (templListView->isRoot(parentItem) || templListView->isChapter(parentItem)) )
        {
            parentItem = (QTreeWidgetItem*) parentItem->parent();
        }
    }

    if( parentItem ) {
      // try to find out which catalog is open/current
      CatalogChapter *chap = static_cast<CatalogChapter*>( templListView->itemData( parentItem ) );
      if( chap ) {
        flosTempl->setChapterID( chap->id().toInt() );
      }
    }

    QTreeWidgetItem *item = templListView->addFlosTemplate(parentItem, flosTempl);
    templListView->scrollToItem( item );
    templListView->setCurrentItem( item );
    openDialog( item, flosTempl, true );
}

bool TemplKatalogView::currentItemToDocPosition( DocPosition& pos )
{
  TemplKatalogListView* listview = static_cast<TemplKatalogListView*>(getListView());
  bool res = false;
  if( listview )
  {
    FloskelTemplate *currTempl = static_cast<FloskelTemplate*> (listview->currentItemData());
    if( currTempl ) {
      // create a new position and offer it to the document manager
      pos.setText( currTempl->getText() );
      pos.setUnit( currTempl->unit() );
      pos.setUnitPrice( currTempl->unitPrice() );
      pos.setAmount( 1.0 );
      res = true;
    }
  }
  return res;
}

CalcPartList TemplKatalogView::currentItemsCalcParts()
{
  TemplKatalogListView* listview = static_cast<TemplKatalogListView*>(getListView());
  CalcPartList cpList;

  if( listview )
  {
    FloskelTemplate *currTempl = static_cast<FloskelTemplate*> (listview->currentItemData());
    if ( currTempl ) {
      cpList = currTempl->getCalcPartsList();
    }
  }
  return cpList;
}


void TemplKatalogView::openDialog( QTreeWidgetItem *listitem, FloskelTemplate *tmpl, bool isNew )
{
    if( ! m_flosDialog )
    {
        m_flosDialog = new FlosTemplDialog(this, false);
        connect( m_flosDialog, SIGNAL(editAccepted( FloskelTemplate* )),
                 this, SLOT( slEditOk(FloskelTemplate*)));
        connect( m_flosDialog, SIGNAL(editRejected( )),
                 this, SLOT( slEditRejected()));
    }
    m_flosDialog->setTemplate( tmpl, m_katalogName, isNew );
    m_editListViewItem = listitem;
    m_flosDialog->refreshPrices();
    m_flosDialog->show();
}

void TemplKatalogView::slEditOk(FloskelTemplate* templ)
{
    KatalogListView *listview = getListView();
    if( !listview ) return;
    TemplKatalogListView *templListView = static_cast<TemplKatalogListView*>(listview);

    if(m_flosDialog ){
      if ( m_flosDialog->templateIsNew() ) {
         TemplKatalog *k = static_cast<TemplKatalog*>( getKatalog( m_katalogName ) );
        if ( k ) k->addNewTemplate( templ );
      }
    }

    if( templListView && m_editListViewItem ) {
      kDebug() << "Edit was ok, refreshing item in list " << m_editListViewItem << endl;
      templListView->setCurrentItem( m_editListViewItem );
      templListView->slFreshupItem( m_editListViewItem, templ, true );
      templListView->scrollToItem( m_editListViewItem );
    }

    m_editListViewItem = 0;
}

void TemplKatalogView::slEditRejected()
{
  kDebug() << "Rejecting Edit!";
  if ( m_editListViewItem ) {
    delete m_editListViewItem;
    m_editListViewItem = 0;
  }

  if( m_flosDialog ) {
    delete m_flosDialog;
    m_flosDialog = 0;
  }
}

void TemplKatalogView::createCentralWidget(QBoxLayout*box, QWidget *w)
{
    kDebug() << "Creating new Listview" << endl;
    m_listview = new TemplKatalogListView( w );
    box->addWidget(m_listview);

    KatalogView::createCentralWidget( box, w );
}

#include "templkatalogview.moc"
