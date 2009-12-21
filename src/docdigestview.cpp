/***************************************************************************
                          docdigestview.cpp  -
                             -------------------
    begin                : Wed Mar 15 2006
    copyright            : (C) 2006 by Klaas Freitag
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
#include <QtGui>

#include <klocale.h>
#include <kdebug.h>
#include <kstandardaction.h>
#include <kaction.h>
#include <ktoolbar.h>
#include <kdialog.h>
#include <kiconloader.h>
#include <kmenu.h>
#include <kcalendarsystem.h>
#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>
#include <kabc/addresseedialog.h>
#include <kabc/addressee.h>

#include "filterheader.h"
#include "docdigestview.h"
#include "documentman.h"
#include "docguardedptr.h"
#include "kraftdoc.h"
#include "defaultprovider.h"


DocDigestView::DocDigestView( QWidget *parent )
: QWidget( parent )
{
  QVBoxLayout *box = new QVBoxLayout;
  setLayout( box );

  box->setMargin( 0 );
  box->setSpacing( 0 );

  QHBoxLayout *hbox = new QHBoxLayout;

  mNewDocButton = new QPushButton( i18n( "Create Document" ) );
  connect( mNewDocButton, SIGNAL( clicked() ), this, SIGNAL( createDocument() ) );

  hbox->addWidget( mNewDocButton );
  hbox->addStretch(1);
  mToolBox = new QToolBox;

  mAllView = new QTreeWidget;
  mLatestView = new QTreeWidget;
  mTimeView = new QTreeWidget;
  connect( mAllView, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
           this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
  connect( mLatestView, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
           this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
  connect( mTimeView, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
           this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
  // mListView->setItemMargin( 5 );
  QPalette palette;
  palette.setColor( QPalette::AlternateBase, QColor("#e0fdd1") );
  
  mAllView->setPalette( palette );
  mLatestView->setPalette( palette );
  mTimeView->setPalette( palette );

  mAllView->setAlternatingRowColors( true );
  mTimeView->setAlternatingRowColors( true );
  mLatestView->setAlternatingRowColors( true );

  mAllMenu = new KMenu( mAllView );
  mAllMenu->setTitle( i18n("Document Actions"));
  mTimelineMenu = new KMenu( mTimeView );
  mTimelineMenu->setTitle( i18n("Document Actions"));
  mLatestMenu = new KMenu( mLatestView );
  mLatestMenu->setTitle( i18n("Document Actions"));


  mAllView->setRootIsDecorated(  true );
  mLatestView->setRootIsDecorated(  true );
  mTimeView->setRootIsDecorated(  true );

  mAllView->setSelectionMode( QAbstractItemView::SingleSelection );
  mLatestView->setSelectionMode( QAbstractItemView::SingleSelection );
  mTimeView->setSelectionMode( QAbstractItemView::SingleSelection );

  mFilterHeader = new FilterHeader( mAllView );
  mFilterHeader->showCount( false );

  connect( mLatestView, SIGNAL( itemDoubleClicked( QTreeWidgetItem*, int ) ),
           this, SLOT( slotDocOpenRequest( QTreeWidgetItem*, int ) ) );

  connect( mLatestView, SIGNAL( currentItemChanged ( QTreeWidgetItem*, QTreeWidgetItem* )),
           this, SLOT( slotCurrentChanged( QTreeWidgetItem*, QTreeWidgetItem* ) ) );

  hbox->addWidget( mFilterHeader );
  hbox->addSpacing( KDialog::marginHint() );

  box->addLayout( hbox );
  //box->addSpacing( KDialog::marginHint() );

  QHBoxLayout *hbox2 = new QHBoxLayout;

  int indx = mToolBox->addItem( mLatestView, i18n("Latest Documents"));
  mToolBox->setItemIcon( indx, KIcon( "get-hot-new-stuff"));
  mToolBox->setItemToolTip(indx, i18n("Shows the latest ten documents"));

  indx = mToolBox->addItem( mAllView, i18n("All Documents"));
  mToolBox->setItemIcon( indx, KIcon( "edit-clear-locationbar-ltr"));
  mToolBox->setItemToolTip(indx, i18n("Shows a complete list of all documents"));

  indx = mToolBox->addItem( mTimeView, i18n("Timelined Documents"));
  mToolBox->setItemIcon( indx, KIcon( "chronometer"));
  mToolBox->setItemToolTip(indx, i18n("Shows all documents along a timeline"));

  hbox2->addWidget( mToolBox );
  hbox2->addSpacing( KDialog::marginHint() );
  box->addLayout( hbox2 );

  mAllView->setColumnCount( 7 );
  mLatestView->setColumnCount( 7 );
  mTimeView->setColumnCount( 7 );
  QStringList cols;
  cols << i18n( "Type" );
  cols << i18n( "Client Name" );
  cols << i18n( "Last Modified" );
  cols << i18n( "Date" );
  cols << i18n( "Whiteboard" );
  cols << i18n( "Project" );
  cols << i18n( "Doc. Number" );

  mLatestView->setHeaderLabels( cols );
  mAllView->setHeaderLabels( cols );
  mTimeView->setHeaderLabels( cols );

}

DocDigestView::~DocDigestView()
{

}

void DocDigestView::slotBuildView()
{
  DocumentMan *docman = DocumentMan::self();
  mLatestView->clear();
  mAllView->clear();
  mTimeView->clear();
  QTreeWidgetItem *item = addChapter( mAllView, i18n( "All Documents" ),
                                      docman->latestDocs( 0 ) );

  mAllDocsParent = item;
  item->setIcon( 0, SmallIcon( "user-identity" ) );
  mLatestView->collapseItem( item );
  mTimeView->collapseItem( item );
  mAllView->collapseItem( item );

  item = addChapter( mTimeView, i18n( "Documents by Time" ), DocDigestList());
  mTimeLineParent = item;
  item->setIcon( 0, SmallIcon( "view-history" ) ); // KDE 4 icon name: view-history
  // mTimeLineParent->collapseItem( item );

  /* create the timeline view */
  DocDigestsTimelineList timeList = docman->docsTimelined();
  DocDigestsTimelineList::iterator it;

  int month = 0;
  int year = 0;
  QTreeWidgetItem *yearItem = 0;

  for ( it = timeList.begin(); it != timeList.end(); ++it ) {
    if ( ( *it ).year() && year != ( *it ).year() ) {
      year = ( *it ).year();

      yearItem = addChapter( mTimeView, QString::number( year ),  DocDigestList());
      // mListView->collapseItem( yearItem );
    }
    month = ( *it ).month();
    const QString monthName =
      DefaultProvider::self()->locale()->calendar()->monthName( month, year ); // , KCalendarSystem::LongName);
    if ( yearItem ) {
      QTreeWidgetItem *mItem = addChapter(  mTimeView, monthName, ( *it ).digests(), yearItem );
      // mListView->collapseItem( mItem );
    }
  }

  item = addChapter( mLatestView, i18n( "Latest Documents" ),  docman->latestDocs( 10 ) );
  mLatestDocsParent = item;
  item->setIcon( 0, SmallIcon( "fork" ) );
  mTimeView->resizeColumnToContents(0);
  
}


QTreeWidgetItem* DocDigestView::addChapter( QTreeWidget* tree, const QString& chapter, DocDigestList list, QTreeWidgetItem *chapParent )
{
  kDebug() << "Adding docview chapter " << chapter << " with " << list.size() << " elems" << endl;

  QTreeWidgetItem *chapIt;
  if ( chapParent ) {
    chapIt = new QTreeWidgetItem( chapParent, QStringList(chapter));
  } else {
    chapIt = new QTreeWidgetItem( tree, QStringList(chapter) );
  }
  tree->expandItem( chapIt );

  DocDigestList::iterator it;
  for ( it = list.begin(); it != list.end(); ++it ) {
    QStringList li;
    li << (*it).type() << (*it).clientName() << ( *it).lastModified() << (*it).date()
        << ( *it ).whiteboard() << ( *it ).projectLabel() << ( *it ).ident();

    QTreeWidgetItem *item = new QTreeWidgetItem( chapIt, li );
    mDocIdDict[item] = (*it).id();

    ArchDocDigestList archDocList = ( *it ).archDocDigestList();
    ArchDocDigestList::iterator archIt;
    for ( archIt = archDocList.begin(); archIt != archDocList.end(); ++archIt ) {
      QStringList li;
      li << i18n("Archived") << QString() << (*archIt).printDateString();
      QTreeWidgetItem *archItem = new QTreeWidgetItem( item, li );
      mArchIdDict[archItem] = (*archIt);
    }
  }
  return chapIt;
}

void DocDigestView::contextMenuEvent( QContextMenuEvent * event )
{
  QTreeWidget *currView = static_cast<QTreeWidget*>(mToolBox->currentWidget());

  if( currView == mLatestView ) {
    mLatestMenu->popup( event->globalPos() );
  } else if( currView == mTimeView ) {
    mTimelineMenu->popup( event->globalPos() );
  } else if( currView == mAllView ) {
    mAllMenu->popup( event->globalPos() );
  }
}

/* Called after the document was saved, thus the doc is complete.
 * The new entry should set selected.
 */
void DocDigestView::slotNewDoc( DocGuardedPtr doc )
{
  QTreeWidgetItem *parent = mLatestDocsParent;
  if ( !doc ) return;

  QTreeWidget *currView = static_cast<QTreeWidget*>(mToolBox->currentWidget());
  if( currView->selectedItems().count() ) {
    QTreeWidgetItem *currItem = currView->selectedItems().first();
    if ( currItem ) currView->setCurrentItem( currItem );
  }

  // insert item into the "latest docs" list. That makes the latest
  // list one item longer, we're not deleting one entry
  QString itemID; // for docSelected signal
  if ( parent ) {
    QTreeWidgetItem *item = new QTreeWidgetItem( parent );
    item->setIcon( 0, SmallIcon( "get-hot-new-stuff" ) );
    setupListViewItemFromDoc( doc, item );
    currView->setCurrentItem( item );
    dbID id = doc->docID();
    if ( id.isOk() ) {
      mDocIdDict[item] = id.toString();
      itemID = id.toString();
    }
  }
  emit docSelected( itemID );

  // Insert new item into the "all documents" list
  parent = mAllDocsParent;
  if ( parent ) {
    QTreeWidgetItem *item = addDocToParent(doc, parent);
    dbID id = doc->docID();
    if ( id.isOk() ) {
      if(item)
        mDocIdDict[item] = id.toString();
    }
  }

  //Insert new item into the "timeline" list
  parent = mTimeLineParent;
  if ( parent )
  {
    QDate docdate = doc.data()->date();
    QString monthname = DefaultProvider::self()->locale()->calendar()->monthName(docdate);
    bool iteminserted = false;
    QTreeWidgetItem *docyear = 0;

    //Iterate over the years
    for(int y=0; y < parent->childCount(); ++y)
    {
      QTreeWidgetItem *year = parent->child(y);
      if(year->text(0).toInt() == docdate.year())
      {
        //If the year of the doc is found, iterate over the months of that year
        for(int m=0; m < year->childCount(); ++m)
        {
           QTreeWidgetItem *month = year->child(m);

           if(month->text(0) == monthname)
           {
             //If the month is found, insert the document and break out of the loop
             addDocToParent(doc, month);
             iteminserted = true;
             break;
           }
        }
        docyear = year;
        break;
      }
    }

    if(!iteminserted)
    {
      //If the item didn't get inserted, it means that the month was not found
      QTreeWidgetItem *docmonth = 0;

      //We need to check if a year was found though
      if(docyear == 0)
      {
        int y=0;
        //The year doesn't exist either. Let's create it at the right spot.
        for(y=0; y < parent->childCount(); ++y)
        {
          QTreeWidgetItem *year = parent->child(y);
          if(year->text(0).toInt() > docdate.year())
            break;
        }

        docyear = new QTreeWidgetItem;
        docyear->setText(0, QString::number(docdate.year()));
        parent->insertChild(y, docyear);

        docmonth = new QTreeWidgetItem;
        docmonth->setText(0, DefaultProvider::self()->locale()->calendar()->monthName( docdate.month(), docdate.year()));
        docyear->addChild(docmonth);
      }

      if(docmonth == 0)
      {
        //Insert the month at the right spot
        int m=0;
        for(m=0; m < docyear->childCount(); ++m)
        {
           QTreeWidgetItem *month = docyear->child(m);
           QDate date = DefaultProvider::self()->locale()->readDate("1 " + month->text(0) , "%e %B");
           if(date.month() > docdate.month())
             break;
        }

        docmonth = new QTreeWidgetItem;
        docmonth->setText(0, DefaultProvider::self()->locale()->calendar()->monthName( docdate.month(), docdate.year()));
        docyear->insertChild(m, docmonth);
      }

      //We know we created a new docmonth, so the item can just get inserted
      QTreeWidgetItem *item = new QTreeWidgetItem;
      setupListViewItemFromDoc( doc, item );
      docmonth->addChild(item);
    }
  }
}

QTreeWidgetItem* DocDigestView::addDocToParent(DocGuardedPtr doc, QTreeWidgetItem *month)
{
  QDate docdate = doc.data()->date();

  //Insert the doc at the right spot
  int d;
  for(d=0; d < month->childCount(); ++d)
  {
     QTreeWidgetItem *document = month->child(d);
     QDate date = DefaultProvider::self()->locale()->readDate(document->text(3));
     if(date < docdate)
       break;
  }

  QTreeWidgetItem *item = new QTreeWidgetItem;
  setupListViewItemFromDoc( doc, item );
  month->insertChild(d, item);
  return item;
}

void DocDigestView::slotUpdateDoc( DocGuardedPtr doc )
{
  if ( !doc ) return;
  const QString docId = doc->docID().toString();

  QMap<QTreeWidgetItem*, QString>::iterator it;

  for(it=mDocIdDict.begin(); it != mDocIdDict.end(); ++it ) {
    QString id = it.value();
    QTreeWidgetItem* item = it.key();


    if ( docId == id ) {
      setupListViewItemFromDoc( doc, item );
    }
  }
}

void DocDigestView::setupListViewItemFromDoc( DocGuardedPtr doc, QTreeWidgetItem* item )
{
  item->setText( 0,  doc->docType() );

  QString clientName;
  KABC::AddressBook *adrBook =  KABC::StdAddressBook::self();
  KABC::Addressee contact;
  if( adrBook ) {
    contact = adrBook->findByUid( doc->addressUid() );
    clientName = contact.realName();
  }
  item->setText( 1,  clientName );
  item->setText( 2, doc->locale()->formatDate( doc->lastModified(), KLocale::ShortDate ) );
  item->setText( 3, doc->locale()->formatDate( doc->date(), KLocale::ShortDate ) );
  item->setText( 4, doc->whiteboard() );
  item->setText( 5, doc->projectLabel() );
  item->setText( 6, doc->ident() );
}

#if 0
void DocDigestView::slotDocViewRequest( QTreeWidgetItem *item )
{
  QString id = mDocIdDict[ item ];
  if( ! id.isEmpty() ) {
    kDebug() << "Opening document " << id;

    emit viewDocument( id );
  }
}
#endif

void DocDigestView::slotDocOpenRequest( QTreeWidgetItem *item, int )
{
  QString id = mDocIdDict[ item ];
  if( ! id.isEmpty() ) {
    kDebug() << "Opening document " << id << endl;

    emit openDocument( id );
  }

  ArchDocDigest archDoc = mArchIdDict[ item ];
  if ( archDoc.archDocId().isOk() ) {
    emit openArchivedDocument( archDoc );
  }
}

ArchDocDigest DocDigestView::currentArchiveDoc() const
{
  QTreeWidget *currView = static_cast<QTreeWidget*>( mToolBox->currentWidget() );
  QTreeWidgetItem *current = currView->selectedItems().first();
  if( current ) {
    return mArchIdDict[current];
  }
  return ArchDocDigest();
}

QString DocDigestView::currentDocumentId()
{
  QString res;
  QTreeWidget *currView = static_cast<QTreeWidget*>( mToolBox->currentWidget() );
  QTreeWidgetItem *current = currView->currentItem();
  if( current ) {
    res = mDocIdDict[current];
  }
  return res;
}

void DocDigestView::slotCurrentChanged( QTreeWidgetItem *item, QTreeWidgetItem* )
{
  dbID id = ( mArchIdDict[item] ).archDocId();
  QString res;
  if ( mDocIdDict.contains( item ) ) {
    emit docSelected( mDocIdDict[item] );
  } else if ( id.isOk() ) {
    emit archivedDocSelected( mArchIdDict[item] );
  }
}

QList<KMenu*> DocDigestView::contextMenus()
{
  QList<KMenu*> menus;
  menus.append( mAllMenu);
  menus.append( mTimelineMenu );
  menus.append( mLatestMenu);

  return menus;
}

#include "docdigestview.moc"
