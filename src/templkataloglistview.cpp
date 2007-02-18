/***************************************************************************
             templkataloglistview  - template katalog listview.
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

#include <klocale.h>
#include <kdebug.h>
#include <kiconloader.h>

#include "templkataloglistview.h"
#include "portal.h"
#include "kraftglobals.h"
#include "katalog.h"
#include "katalogman.h"
#include "kataloglistview.h"
#include "materialcalcpart.h"
#include "stockmaterial.h"
#include "templkatalog.h"
#include "zeitcalcpart.h"

TemplKatalogListView::TemplKatalogListView(QWidget *w)
    : KatalogListView(w),
      mShowCalcParts( true )
{
    addColumn( i18n("Template"));

    int priceCol = addColumn( i18n("Price"));
    setColumnWidthMode(0, Manual);
    setColumnWidth(0, 500);
    addColumn( i18n("Calc. Type"));
    // addColumn( i18n("ID"));
    setSortColumn( -1 );
    setColumnAlignment ( priceCol, Qt::AlignRight);
}

/*
 * This class adds a complete catalog and fills the view. It gets the
 * catalog from KatalogMan, iterates over the catalog chapters and
 * fills in the templates.
 */
void TemplKatalogListView::addCatalogDisplay( const QString& katName )
{
    KatalogListView::addCatalogDisplay(katName);

    TemplKatalog* catalog = static_cast<TemplKatalog*>(KatalogMan::self()->getKatalog(katName));

    if ( !catalog ) {
      kdError() << "Could not load catalog " << katName << endl;
      return;
    }

    setupChapters();

    const QStringList chapters = catalog->getKatalogChapters();
    for ( QStringList::ConstIterator it = chapters.begin(); it != chapters.end(); ++it ) {
        QString chapter = *it;
        KListViewItem *katItem = chapterItem(chapter);
        // kdDebug() << "KatItem is " << katItem << endl;
        FloskelTemplateList katList = catalog->getFlosTemplates(chapter);
        // kdDebug() << "Items in chapter " << chapter << ": " << katList.count() << endl;
        FloskelTemplateListIterator flosIt( katList );
        FloskelTemplate *tmpl;

        /* iterate over all templates */
        while ( (tmpl = flosIt.current()) != 0 ) {
            /* create a new item as the child of katalog entry */
            addFlosTemplate( katItem, tmpl );
            if ( mShowCalcParts )
              addCalcParts( tmpl );
            ++flosIt;
        }
    }
}

/*
 * add a single template to the view with setting icon etc.
 */
KListViewItem* TemplKatalogListView::addFlosTemplate( KListViewItem *parentItem, FloskelTemplate *tmpl )
{
    if( ! parentItem ) parentItem = m_root;
    KListViewItem *listItem = new KListViewItem( parentItem );
    slFreshupItem( listItem, tmpl);
    tmpl->setListViewItem( listItem );

    listItem->setMultiLinesEnabled(true);

    if( tmpl->calcKind() == ManualPrice )
    {
        listItem->setPixmap(0, SmallIcon("roll"));
    }
    else
    {
        listItem->setPixmap(0, SmallIcon("kcalc"));
    }
    // store the connection between the listviewitem and the template in a dict.
    m_dataDict.insert( listItem, tmpl );

    return listItem;
}

void TemplKatalogListView::slFreshupItem( QListViewItem *item, FloskelTemplate *tmpl, bool remChildren )
{
    if( !(item && tmpl) ) return;

    Geld g     = tmpl->einheitsPreis();
    QString ck = tmpl->calcKindString();
    QString t  = Portal::textWrap(tmpl->getText(), 60);

    item->setText( 0, t );
    QString h;
    h = QString( "%1 / %2" ).arg( g.toString() ).arg( tmpl->einheit().einheitSingular() );
    item->setText( 1,  h );
    item->setText( 2, ck );
    // item->setText( 4, QString::number(tmpl->getTemplID()));

    if( remChildren ) {
      /* remove all children and insert them again afterwards.
        * That updates the view
      */
        QListViewItem *it = item->firstChild();
        if( it )  {
            QListViewItem *nextIt = it->nextSibling();
            delete it;

            while( nextIt ) {
                it = nextIt;
                nextIt = it->nextSibling();
                delete it;
            }
        }

        addCalcParts(tmpl); // Insert to update the view again.
    }
}


void TemplKatalogListView::addCalcParts( FloskelTemplate *tmpl )
{
    KListViewItem *item = tmpl->getListViewItem();

    if( ! item ) return;

    CalcPartList parts = tmpl->getCalcPartsList();

    CalcPart *cp;
    for ( cp = parts.first(); cp; cp = parts.next() )
    {
        QString title = cp->getName();
        QString type = cp->getType();
        kdDebug() << "Type is " << type << endl;
        if( type  == KALKPART_TIME ) {
            ZeitCalcPart *zcp = static_cast<ZeitCalcPart*>(cp);
            StdSatz stdsatz = zcp->getStundensatz();
            title = QString( "%1, %2 Min. %3" ).arg( cp->getName() )
                    .arg( QString::number( zcp->getMinuten() ) )
                    .arg( stdsatz.getName() );
        }

        KListViewItem *cpItem =  new KListViewItem( item, title,
                                                    cp->kosten().toString(), cp->getType() );


        /* in case of material, add items for the materials calculated for the
        * template
        */
        if( type == KALKPART_MATERIAL )
        {
            MaterialCalcPart *mcp = static_cast<MaterialCalcPart*>( cp );
            StockMaterialList mats =  mcp->getCalcMaterialList();

            StockMaterialListIterator it( mats );

            StockMaterial *mat;
            while ( (mat = it.current()) != 0 )
            {
                ++it;
                Geld g = mcp->getPriceForMaterial(mat);
                QString t = mat->name();
                double usedAmount = mcp->getCalcAmount(mat);
                Einheit e = mat->getUnit();

                t = QString( "%1 %2 of %3 %4 %5" ).arg( usedAmount )
                    .arg( e.einheit( usedAmount ) )
                    .arg( mat->getAmountPerPack() )
                    .arg( e.einheit( mat->getAmountPerPack() ) )
                    .arg( t );
                (void) new KListViewItem( cpItem, t, g.toString() );
            }
        }
    }
}

void TemplKatalogListView::setShowCalcParts( bool on )
{
  mShowCalcParts = on;
}

bool TemplKatalogListView::showCalcParts()
{
  return mShowCalcParts;
}

TemplKatalogListView::~TemplKatalogListView()
{
}

DocPosition TemplKatalogListView::itemToDocPosition( QListViewItem *it )
{
  DocPosition pos;
  if ( ! it ) {
    it = currentItem();
  }

  if ( ! it ) return pos;

  FloskelTemplate *flos = static_cast<FloskelTemplate*>( m_dataDict[ it ] );

  if ( flos ) {
    pos.setText( flos->getText() );
    pos.setUnit( flos->einheit() );
    pos.setUnitPrice( flos->einheitsPreis() );
  } else {
    kdDebug() << "Can not find a template for the item" << endl;
  }

  return pos;
}

