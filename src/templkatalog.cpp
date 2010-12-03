/***************************************************************************
                          flostempllist.cpp  -
                             -------------------
    begin                : Son Feb 8 2004
    copyright            : (C) 2004 by Klaas Freitag
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

#include <QSqlQuery>
#include <QStringList>
#include <qdom.h>
#include <QFile>
#include <QDir>

#include <kdebug.h>
#include <kfiledialog.h>
#include <klocale.h>

#include "floskeltemplate.h"
#include "dbids.h"
#include "templkatalog.h"
#include "kraftdb.h"
#include "unitmanager.h"
#include "timecalcpart.h"
#include "fixcalcpart.h"
#include "materialcalcpart.h"
#include "geld.h"
#include "katalog.h"

/** constructor of a katalog, which is only a list of Floskel templates.
 *  A name must be given, which is displayed for the root element in the
 *
 */

TemplKatalog::TemplKatalog( const QString& name )
    : Katalog( name )
{

}

TemplKatalog::~TemplKatalog()
{

}

void TemplKatalog::reload( dbID id)
{
  FloskelTemplate *templ=0;
  //Find the template we want to reload in the templatelist
  for(int i=0; i < m_flosList.count(); ++i)
  {
    templ = m_flosList[i];
    if(templ->getTemplID() == id.toInt())
      break;
  }

  if(templ)
  {
    QSqlQuery q("SELECT unitID, TemplID, chapterID, Preisart, EPreis, modifyDatum, enterDatum, Floskel, Gewinn, zeitbeitrag FROM Catalog WHERE TemplID=:TemplID");
    q.bindValue(":TemplID", id.toInt());
    q.exec();

    if(q.next())
    {
      //templ->setEinheitId(q.value(0).toInt());
      kDebug() << "Reloading template number " << q.value(1) << endl;
      templ->setChapterID(q.value(2).toInt());
      //templ->setCalculationType(q.value(3).toInt());
      templ->setManualPrice(q.value(4).toDouble());
      templ->setText( q.value(7).toString() );
      templ->setBenefit( q.value(8).toDouble());
      templ->setHasTimeslice( q.value(9).toBool() );

      templ->clearCalcParts();
      loadCalcParts( templ );
    }
  }
}

int TemplKatalog::load()
{
  Katalog::load();
  int cnt = 0;

  QString chapIdList;
  bool firstOne = true;
  foreach( CatalogChapter chap, mChapters ) {
    if( !firstOne ) {
      chapIdList += ",";
    } else {
      firstOne = false;
    }
    chapIdList += chap.id().toString();
  }

  kDebug() << "The chapterIdList: " << chapIdList;
  QSqlQuery q("SELECT unitID, TemplID, chapterID, Preisart, EPreis, modifyDatum, enterDatum, "
              "Floskel, Gewinn, zeitbeitrag, lastUsed, useCounter FROM Catalog WHERE chapterID IN( " + chapIdList + ") "
              "ORDER BY chapterID, sortKey" );
  q.exec();
  while ( q.next() ) {
    cnt++;
    int einheit = q.value(0).toInt();
    int templID = q.value(1).toInt();
    kDebug() << "Loading template number " << templID << endl;
    int chapID = q.value(2).toInt();
    // int sortID = cur.value( "sortKey" ).toInt();
    int calcKind = q.value(3).toInt();
    double g = q.value(4).toDouble();

    Geld preis(g);
    /* Only for debugging: */
    if( templID == 272 ) {
      kDebug() << "Geld ist " << preis.toString( *( &mLocale ) ) << " from g-value " << g << endl;
    }

    QDateTime modDt;
    QString modDate = q.value(5).toString();
    /* modifyDatum ist TIMESTAMP und den gibt mysql offensichtlich mit einem T im
         * String zurck */
    if( modDate[10] == 'T' ) {
      modDate = modDate.replace( 10, 1, QChar(' ') );
      modDt = QDateTime::fromString(modDate, Qt::ISODate );
    }

    QDateTime enterDt = q.value(6).toDateTime();

    kDebug() << "Chapter ID is " << chapID << endl;

    FloskelTemplate *flos = new FloskelTemplate( templID,
                                                 q.value(7).toString(),
                                                 einheit, chapID, calcKind );
    flos->setEnterDate( enterDt );
    flos->setModifyDate( modDt );
    // flos->setSortKey( sortID );
    flos->setBenefit( q.value(8).toDouble());
    flos->setManualPrice( preis );
    bool tslice = q.value(9).toInt() > 0;
    flos->setHasTimeslice( tslice );

    flos->setLastUsedDate( q.value(10).toDateTime() );
    flos->setUseCounter( q.value(11).toInt() );

    loadCalcParts( flos );

    // FIXME KDE4: Append to list sorted.
    m_flosList.append(flos);
  }
  return cnt;
}

int TemplKatalog::addNewTemplate( FloskelTemplate *tmpl )
{
  int re = -1;

  if ( tmpl ) {
    m_flosList.append( tmpl );
    re = m_flosList.count();
  }
  return re;
}

int TemplKatalog::loadCalcParts( FloskelTemplate *flos )
{
  int cnt = 0;

  cnt = loadTimeCalcParts( flos );
  cnt += loadFixCalcParts( flos );
  cnt += loadMaterialCalcParts(flos);
  return cnt;
}

int TemplKatalog::loadTimeCalcParts( FloskelTemplate *flos )
{
  if( ! flos ) return(0);
  int cnt = 0;

  QSqlQuery q;
  q.prepare("SELECT TCalcID, TemplID, name, minutes, percent, stdHourSet, allowGlobal FROM CalcTime WHERE TemplID=:TemplID");
  q.bindValue(":TemplID", QString::number( flos->getTemplID()));
  q.exec();

  while( q.next() )
  {
    cnt++;
    int tcalcid = q.value(0).toInt();
    int templid = q.value(1).toInt();
    QString name = q.value(2).toString();
    int minutes = q.value(3).toInt();
    int prozent = q.value(4).toInt();
    int hourSet = q.value(5).toInt();
    bool globAllowed = q.value(6).toInt() > 0;

    TimeCalcPart *zcp = new TimeCalcPart( name, minutes, prozent );
    zcp->setGlobalStdSetAllowed( globAllowed );
    zcp->setStundensatz( StdSatzMan::self()->getStdSatz(hourSet) );

    zcp->setDbID( dbID(tcalcid));
    zcp->setTemplID( dbID(templid));
    zcp->setDirty( false );
    flos->addCalcPart( zcp );
  }

  return cnt;
}

int TemplKatalog::loadMaterialCalcParts( FloskelTemplate *flos )
{
  if( ! flos ) return(0);
  int cnt = 0;

  QSqlQuery q;
  q.prepare("SELECT MCalcID, TemplID, materialID, percent, amount FROM CalcMaterials WHERE TemplID=:TemplID");
  q.bindValue(":TemplID", QString::number( flos->getTemplID()));
  q.exec();

  while( q.next() )
  {
    cnt++;
    long mcalcID = q.value(0).toLongLong();
    int templid = q.value(1).toInt();
    long   matID  = q.value(2).toLongLong();
    int procent = q.value(3).toInt();
    double amount = q.value(4).toDouble();


    MaterialCalcPart *mPart = new MaterialCalcPart( mcalcID, matID, procent, amount );
    mPart->setDbID( dbID(mcalcID));
    mPart->setTemplID( dbID(templid));
    mPart->setDirty( false );
    flos->addCalcPart( mPart );
  }

  return cnt;
}

int TemplKatalog::loadFixCalcParts( FloskelTemplate *flos )
{
  if( ! flos ) return(0);
  int cnt = 0;

  QSqlQuery q;
  q.prepare("SELECT name, amount, percent, FCalcID, TemplID, price FROM CalcFixed WHERE TemplID=:TemplID");
  q.bindValue(":TemplID", QString::number( flos->getTemplID()));
  q.exec();

  while( q.next() )
  {
    cnt++;
    QString name  = q.value(0).toString();
    double amount = q.value(1).toDouble();
    int percent   = q.value(2).toInt();
    int tcalcid = q.value(3).toInt();
    int templid = q.value(4).toInt();

    double g      = q.value(5).toDouble();
    Geld price(g); //     = (int) g; // FIXME: proper handling of money here.

    FixCalcPart *fcp = new FixCalcPart( name, price, percent );
    fcp->setMenge( amount );
    fcp->setDbID( dbID(tcalcid));
    fcp->setTemplID( dbID(templid));
    fcp->setDirty( false );
    flos->addCalcPart( fcp );
  }

  return cnt;
}


FloskelTemplateList TemplKatalog::getFlosTemplates( const CatalogChapter& chapter )
{
  FloskelTemplateList resultList;
  int chap = chapter.id().toInt();

  if( m_flosList.count() == 0 )
  {
    kDebug() << "Empty katalog list - loading!" << endl;
    load();
  }

  FloskelTemplateListIterator it(m_flosList);
  FloskelTemplate *tmpl;

  while( it.hasNext() )
  {
    tmpl = it.next();

    int haveChap = tmpl->getChapterID();

    // kDebug() << "Searching for chapter " << chapter << " with ID " << chap << " and have " << haveChap << endl;
    if( haveChap == chap )
    {
      resultList.append( tmpl );
    }
  }
  return resultList;
}


int TemplKatalog::load( const QString& /* chapter */ )
{
  return 0;
}


void TemplKatalog::writeXMLFile()
{
  QString filename = KFileDialog::getSaveFileName( QDir::homePath(),
                                                   "*.xml", 0, i18n("Export XML Katalog"));
  if(filename.isEmpty()) return;

  QDomDocument doc = toXML();

  QFile file( filename );
  if( file.open( QIODevice::WriteOnly ) )
  {
    QTextStream ts( &file );
    ts << doc.toString();

    file.close();
  }

}

QDomDocument TemplKatalog::toXML()
{

  QDomDocument doc("catalog");
  QDomElement root = doc.createElement("catalog");
  doc.appendChild(root);
  QDomElement elem = doc.createElement("catalogname");
  QDomText text = doc.createTextNode(m_name);
  elem.appendChild(text);
  root.appendChild(elem);

  QStringList allSets = StdSatzMan::self()->allStdSaetze();
  for ( QStringList::Iterator it = allSets.begin(); it != allSets.end(); ++it ) {
    QDomElement set = doc.createElement("hourset");
    QDomElement elem = doc.createElement("name");
    QDomText tname = doc.createTextNode(*it);
    elem.appendChild(tname);
    set.appendChild(elem);

    QDomElement rateelem = doc.createElement("rate");
    StdSatz satz = StdSatzMan::self()->getStdSatz(*it);
    Geld g = satz.getPreis();
    QDomText rname = doc.createTextNode(g.toString( mLocale ));
    rateelem.appendChild(rname);
    set.appendChild(rateelem);

    root.appendChild(set);
  }

  QList<CatalogChapter> chaps = getKatalogChapters();
  foreach( CatalogChapter theChapter, chaps ) {
    QString chapter = theChapter.name();
    QDomElement chapElem = doc.createElement("chapter");
    QDomElement chapName = doc.createElement("chaptername");
    text = doc.createTextNode(chapter);
    chapName.appendChild(text);
    chapElem.appendChild(chapName);
    root.appendChild(chapElem);

    FloskelTemplateList templs = getFlosTemplates(theChapter);
    FloskelTemplateListIterator it(templs);

    // FIXME: XML export!
  }
  return doc;
}


int TemplKatalog::getEntriesPerChapter( const CatalogChapter& chapter)
{
  int cnt = 0;

  QString q( QString("SELECT count(*) FROM katalog WHERE chapterID=%1" ).arg( chapter.id().toInt() ) );
  QSqlQuery query( q );

  while ( query.next() ) {
    cnt = query.value(0).toInt();
  }
  return cnt;
}
