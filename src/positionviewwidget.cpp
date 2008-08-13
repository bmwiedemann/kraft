/***************************************************************************
        positionviewwidget - inherited class for doc position views.
                             -------------------
    begin                : 2006-02-20
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

#include <qlabel.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <qcolor.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <kglobal.h>
#include <klocale.h>
#include <knuminput.h>
#include <ktextedit.h>
#include <kpopupmenu.h>
#include <kiconloader.h>
#include <qwidgetstack.h>

#include "extendedcombo.h"
#include "docposition.h"
#include "positionviewwidget.h"
#include "unitmanager.h"
#include "geld.h"
#include "kraftsettings.h"
#include "defaultprovider.h"
#include "kraftdb.h"
#include "positiontagdialog.h"
#include "tagman.h"
#include <qregexp.h>

PositionViewWidget::PositionViewWidget()
 : positionWidget(),
   mModified( false ),
   m_skipModifiedSignal( false ),
   mToDelete(false),
   mOrdNumber(0),
   mPositionPtr( 0 ),
   mExecPopup( new KPopupMenu( this ) ) ,
   mStateSubmenu( 0 ),
   mState( Active ),
   mKind( Normal ),
   mLocale( 0 )
{
  m_sbUnitPrice->setMinValue( -99999.99 );
  m_sbUnitPrice->setMaxValue( 99999.99 );
  m_sbUnitPrice->setPrecision( 2 );

  m_sbAmount->setMinValue( -99999.99 );
  m_sbAmount->setMaxValue( 99999.99 );
  m_sbAmount->setPrecision( 2 );

  mDiscountPercent->setMinValue( -9999.99 );
  mDiscountPercent->setMaxValue( 9999.99 );
  mDiscountPercent->setPrecision( 2 );

  pbExec->setToggleButton( false );
  pbTagging->setToggleButton( false );
  pbTagging->setPixmap( SmallIcon( "flag" ) );

  connect( m_sbAmount, SIGNAL( valueChanged( double )),
             this, SLOT( slotRefreshPrice( ) ) );
  connect( m_sbUnitPrice, SIGNAL( valueChanged( double )),
             this, SLOT( slotRefreshPrice( ) ) );
  connect( mDiscountPercent, SIGNAL( valueChanged( double ) ),
           this, SLOT( slotRefreshPrice() ) );
  connect( pbExec, SIGNAL( pressed() ),     this,  SLOT( slotExecButtonPressed() ) );
  connect( pbTagging,  SIGNAL( pressed() ), this,  SLOT( slotTaggingButtonPressed() ) );


  /* modified signals */
  connect( m_cbUnit,      SIGNAL( activated(int) ), this,      SLOT( slotModified() ) );
  // connect( m_teFloskel,   SIGNAL( textChanged() ), this,       SLOT( slotModified() ) );
  // teFloskel is already connected in ui file
  connect( m_sbAmount,    SIGNAL( valueChanged(double)), this, SLOT( slotModified() ) );
  connect( m_sbUnitPrice, SIGNAL( valueChanged(double)), this, SLOT( slotModified() ) );
  connect( mDiscountPercent, SIGNAL( valueChanged( double ) ), this, SLOT( slotModified() ) );

  mExecPopup->insertTitle( i18n("Position Actions") );

  // state submenu:
  mStateSubmenu = new QPopupMenu;
  mStateSubmenu->insertItem( i18n( "Normal" ), this, SIGNAL( positionStateNormal() ) );
  mStateSubmenu->insertItem( SmallIconSet( "alternative" ),
                            i18n( "Alternative" ), this, SIGNAL( positionStateAlternative() ) );
  mStateSubmenu->insertItem( SmallIconSet( "demand" ),
                            i18n( "On Demand" ), this, SIGNAL( positionStateDemand() ) );
  mExecPopup->insertItem( i18n( "Position Kind" ), mStateSubmenu );

  mExecPopup->insertSeparator();


  mExecPopup->insertSeparator();

  mExecPopup->insertItem(  SmallIconSet("up"),
                           i18n("Move Up"),         this, SIGNAL( moveUp() ) );
  mExecPopup->insertItem(  SmallIconSet("down"),
                           i18n("Move Down"),       this, SIGNAL( moveDown() ) );
  mLockId = mExecPopup->insertItem(  SmallIconSet("encrypted"),
                           i18n("Lock Position"),   this, SIGNAL( lockPosition() ) );
  mUnlockId = mExecPopup->insertItem(  SmallIconSet("decrypted"),
                           i18n("Unlock Position"), this, SIGNAL( unlockPosition() ) );
  mDeleteId = mExecPopup->insertItem(  SmallIconSet("remove"),
                           i18n("Delete Position"), this, SIGNAL( deletePosition() ) );


  connect( this, SIGNAL( positionStateNormal() ), this, SLOT( slotSetPositionNormal() ) );
  connect( this, SIGNAL( positionStateAlternative() ), this, SLOT( slotSetPositionAlternative() ) );
  connect( this, SIGNAL( positionStateDemand() ), this, SLOT( slotSetPositionDemand() ) );


  connect( this, SIGNAL( lockPosition() ),   this, SLOT( slotLockPosition() ) );
  connect( this, SIGNAL( unlockPosition() ), this, SLOT( slotUnlockPosition() ) );

  connect( mExecPopup, SIGNAL( aboutToShow() ), this, SLOT( slotMenuAboutToShow() ) );
  connect( mExecPopup, SIGNAL( aboutToHide() ), this, SLOT( slotMenuAboutToHide() ) );

  mExecPopup->setItemEnabled( mUnlockId, false );
  lStatus->setPixmap( QPixmap() );
  lKind->setPixmap( QPixmap() );
}

void PositionViewWidget::setDocPosition( DocPositionBase *dp, KLocale* loc )
{
  if( ! dp ) {
    kdError() << "setDocPosition got empty position!" << endl;
    return;
  }

  DocPosition *pos = static_cast<DocPosition*>(dp);

  mPositionPtr = pos;

  m_skipModifiedSignal = true;

  m_teFloskel->setText( pos->text() );

  lStatus->hide();
  lKind->hide();

  setLocale( loc );
  AttributeMap amap = dp->attributes();

  if( dp->type() == DocPositionBase::Position ) {
    positionDetailStack->raiseWidget( positionPage );

    m_sbAmount->blockSignals( true );
    m_sbAmount->setValue( pos->amount() );
    m_sbAmount->blockSignals( false );
    m_cbUnit->setCurrentText( pos->unit().einheitSingular() );

    m_sbUnitPrice->blockSignals( true );
    m_sbUnitPrice->setValue( pos->unitPrice().toDouble() );
    m_sbUnitPrice->blockSignals( false );

    if ( amap.contains( DocPosition::Kind ) ) {
      Attribute kind = amap[DocPosition::Kind];
      const QString kindStr = kind.value().toString();
      if ( kindStr == kindString( Alternative ) ) {
        slotSetPositionAlternative();
      } else if ( kindStr == kindString( Demand ) ) {
        slotSetPositionDemand();
      } else {
        kdDebug() << "Unknown position kind!" << endl;
      }
    }
    kdDebug() << "Setting position ptr. in viewwidget: " << pos << endl;
  } else if ( dp->type() == DocPositionBase::ExtraDiscount ) {
    positionDetailStack->raiseWidget( discountPage );
    // kdDebug() << " " << dp->type()<< endl;
    Attribute discount = amap[DocPosition::Discount];
    mDiscountPercent->setValue( discount.value().toDouble() );

    QString selTag;
    if ( amap.contains( DocPosition::ExtraDiscountTagRequired ) ) {
      Attribute tagSelector = amap[DocPosition::ExtraDiscountTagRequired];
      selTag = tagSelector.value().toString();
    }

    /* Fill and set the extra discount selection combo */
    const QString allPos = i18n( "all items" );
    mDiscountTag->insertEntry( allPos, i18n( "Overall Position Discout" ) );
    QStringList taglist = TagTemplateMan::self()->allTagTemplates();
    QString currentEntry = allPos;

    for ( QStringList::Iterator tagIt = taglist.begin(); tagIt != taglist.end(); ++tagIt ) {
      QString tagger;
      tagger = i18n( "%1-tagged items" ).arg( *tagIt );
      mDiscountTag->insertEntry( tagger, i18n( "sum up only items marked with '%1'" ).arg( *tagIt ) );
      if ( selTag == *tagIt ) {
        currentEntry = tagger;
      }
    }
    mDiscountTag->setCurrentText( currentEntry );
  } else {
    kdDebug() << "unknown doc position type " << dp->type()<< endl;
  }
  slotSetOverallPrice( currentPrice() );

  // set tags marked
  mTags = dp->tags();
  slotUpdateTagToolTip();

  m_skipModifiedSignal = false;
}

void PositionViewWidget::slotUpdateTagToolTip()
{
  QString tip;
  bool first = true;

  if ( mTags.count() == 1 ) {
    tip = i18n( "Tag: %1" ).arg( mTags.first() );

  } else if ( mTags.count() > 1 ) {
    tip = i18n( "Tags:<ul>" );
    for ( QStringList::Iterator it = mTags.begin(); it != mTags.end(); ++it ) {
      if ( first ) {
        tip += QString( "<li>%1</li>" ).arg( *it );
        first = false;
      } else {
        tip += QString( "<li>%1</li>" ).arg( *it );
      }
    }
    tip += "</ul>";
  } else {
    tip = i18n( "No tags assigned yet." );
  }

  QToolTip::add( pbTagging, tip );
}

QString PositionViewWidget::extraDiscountTagRestriction()
{
  QString selection = mDiscountTag->currentText();
  QRegExp rx( "\\b(.+)-tagged\\b" );
  if ( rx.search( selection ) > -1 ) {
    return rx.cap( 1 );
  }
  return QString();
}

void PositionViewWidget::setLocale( KLocale *loc )
{
  mLocale = loc;
  const QString currSymbol = mLocale->currencySymbol();
  m_sbUnitPrice->setPrefix( currSymbol + " " );
  slotSetOverallPrice( currentPrice() );
}

void PositionViewWidget::slotTaggingButtonPressed()
{
  kdDebug() << "opening tagging dialog" << endl;

  PositionTagDialog dia( 0 );

  dia.setPositionTags( mTags );
  if ( dia.exec() ) {
    mTags = dia.getSelectedTags();
    slotUpdateTagToolTip();
    kdDebug() << "Selected tags: " << mTags.join( ", " ) << endl;
  }
}


void PositionViewWidget::slotExecButtonPressed()
{
  kdDebug() << "Opening Context Menu over exec button" << endl;

  // set bg-color
  mExecPopup->popup( QWidget::mapToGlobal( pbExec->pos() ) );

}

void PositionViewWidget::slotMenuAboutToShow()
{
  // setPaletteBackground( QColor( "blue" ) );
  setBackgroundMode( Qt::PaletteMid );
}

void PositionViewWidget::slotMenuAboutToHide()
{
  kdDebug() << "Set normal again" << endl;
  setBackgroundMode( Qt::PaletteBackground );
}

void PositionViewWidget::slotLockPosition( )
{
  slotSetState( Locked );
}

void PositionViewWidget::slotUnlockPosition( )
{
  slotSetState( Active );
}

void PositionViewWidget::slotEnableKindMenu( bool s )
{
  mStateSubmenu->setEnabled( s );
}

QString PositionViewWidget::stateString( const State& state ) const
{
  QString str;

  if( state == Active ) {
    str = i18n( "Active" );
  } else if( state == New ) {
    str = i18n( "New" );
  } else if( state == Deleted ) {
    str = i18n( "Deleted" );
  } else if( state == Locked ) {
    str = i18n( "Locked" );
  } else {
    str = i18n( "Unknown" );
  }
  return str;
}

void PositionViewWidget::slotSetState( State state )
{
  mState = state;
  kdDebug() << "Setting new widget state " << stateString( state ) << endl;
  if( state == Active ) {
    mExecPopup->setItemEnabled( mLockId, true);
    mExecPopup->setItemEnabled( mUnlockId, false );

    lStatus->hide();
    lStatus->setPixmap( QPixmap() );
    mToDelete = false;
    slotSetEnabled( true );
  } else if( state == New ) {
    lStatus->setPixmap( SmallIcon( "filenew" ) );
    lStatus->show();
  } else if( state == Deleted ) {
    lStatus->setPixmap( SmallIcon( "remove" ) );
    lStatus->show();
    mToDelete = true;
    slotSetEnabled( false );
  } else if( state == Locked ) {
    mExecPopup->setItemEnabled( mLockId, false );
    mExecPopup->setItemEnabled( mUnlockId, true );
    slotSetEnabled( false );
    lStatus->setPixmap( SmallIcon( "encrypted" ) );
    lStatus->show();
  }
}

void PositionViewWidget::setOrdNumber( int o )
{
  mOrdNumber = o;
  m_labelPosition->setText( QString("%1.").arg( mOrdNumber ) );
}

void PositionViewWidget::slotSetEnabled( bool doit )
{
  if( !doit ) {
    m_sbAmount->setEnabled( false );
    m_sbUnitPrice->setEnabled( false );
    m_labelPosition->setEnabled( false );
    m_teFloskel->setEnabled( false );
    m_sumLabel->setEnabled( false );
    m_cbUnit->setEnabled( false );
  } else {
    m_sbAmount->setEnabled( true );
    m_sbUnitPrice->setEnabled( true );
    m_labelPosition->setEnabled( true );
    m_teFloskel->setEnabled( true );
    m_sumLabel->setEnabled( true );
    m_cbUnit->setEnabled( true );
  }
}

Geld PositionViewWidget::currentPrice()
{
  Geld sum;
  if ( mKind == Normal ) {
    double amount = m_sbAmount->value();
    Geld g( m_sbUnitPrice->value() );
    sum = g * amount;
  }
  return sum;
}

Geld PositionViewWidget::unitPrice()
{
  Geld p(  m_sbUnitPrice->value() );
  return p;
}

void PositionViewWidget::slotRefreshPrice()
{
  const Geld sum = currentPrice();
  slotSetOverallPrice( sum );
  emit priceChanged( sum );
}

void PositionViewWidget::slotSetOverallPrice( Geld g )
{
  // if ( mPositionPtr->type() == DocPosition::ExtraDiscount ) {
  //   m_sumLabel->setText( "--" );
  // } else {
    m_sumLabel->setText( g.toString( mLocale ) );
    // }
}

void PositionViewWidget::slotModified()
{
  // if( mModified ) return;
  if( m_skipModifiedSignal ) return;
  kdDebug() << "Modified Position!" << endl;
  QColor c( "red" );
  m_labelPosition->setPaletteForegroundColor( c );
  mModified = true;
  emit positionModified();
}

PositionViewWidget::~PositionViewWidget()
{
}

PositionViewWidgetList::PositionViewWidgetList()
  : QPtrList<PositionViewWidget>()
{
  setAutoDelete( true );
}

PositionViewWidget* PositionViewWidgetList::widgetFromPosition( DocPositionGuardedPtr ptr)
{
  PositionViewWidget *pvw = 0;

  for( pvw = first(); pvw; pvw = next() ) {
    if( pvw->position() == ptr ) return pvw;
  }
  return 0;
}

Geld PositionViewWidgetList::nettoPrice()
{
  PositionViewWidget *pvw = 0;
  Geld res;

  for( pvw = first(); pvw; pvw = next() ) {
    res += pvw->currentPrice();
  }
  return res;
}

void PositionViewWidget::slotSetPositionNormal()
{
  lKind->hide();
  lKind->setPixmap( QPixmap() );
  mKind = Normal;

  cleanKindString();
  slotRefreshPrice();
  emit positionModified();
}

void PositionViewWidget::cleanKindString()
{
  QString current = m_teFloskel->text();
  bool touched = false;

  if ( current.startsWith( kindLabel( Alternative ) ) ) {
    current.remove( 0, QString( kindLabel( Alternative ) ).length() );
    touched = true;
  } else if ( current.startsWith( kindLabel( Demand ) ) ) {
    current.remove( 0, QString( kindLabel( Demand ) ).length() );
    touched = true;
  }

  if ( touched ) {
    m_teFloskel->setText( current );
  }
}

void PositionViewWidget::slotSetPositionAlternative()
{
  lKind->show();
  QToolTip::add( lKind, i18n( "This is an alternative position."
                              " Use the position toolbox to change." ) );
  lKind->setPixmap( SmallIcon( "alternative" ) );
  mKind = Alternative;
  slotRefreshPrice();

  cleanKindString();

  m_teFloskel->insertAt( kindLabel( Alternative ), 0, 0 );

  emit positionModified();
}

void PositionViewWidget::slotSetPositionDemand()
{
  lKind->show();
  QToolTip::add( lKind, i18n( "This is a as required position. "
                              "Use the position toolbox to change." ) );
  lKind->setPixmap( SmallIcon( "demand" ) );
  mKind = Demand;
  slotRefreshPrice();

  cleanKindString();
  m_teFloskel->insertAt( kindLabel( Demand ), 0, 0 );

  emit positionModified();
}

// The technical label
QString PositionViewWidget::kindString( Kind k ) const
{
  Kind kind = k;

  if ( kind == Invalid ) kind = mKind;

  if ( kind == Normal )      return QString::fromLatin1( "Normal" );
  if ( kind == Demand )      return QString::fromLatin1( "Demand" );
  if ( kind == Alternative ) return QString::fromLatin1( "Alternative" );

  return QString::fromLatin1( "unknown" );
}

// The label that is prepended to a positions text
QString PositionViewWidget::kindLabel( Kind k ) const
{
  Kind kind = k;

  if ( kind == Invalid ) kind = mKind;

  QString re;
  if ( kind == Normal ) {
    re = KraftSettings::self()->normalLabel();
    if ( re.isEmpty() ) re = i18n( "Normal" );
  }
  if ( kind == Demand ) {
    re = KraftSettings::self()->demandLabel();
    if ( re.isEmpty() ) re = i18n( "Demand" );
  }
  if ( kind == Alternative ) {
    re = KraftSettings::self()->alternativeLabel();
    if ( re.isEmpty() ) re = i18n( "Alternative" );
  }

  if ( ! re.endsWith( ": " ) ) {
    re += QString::fromLatin1( ": " );
  }
  return re;
}
#include "positionviewwidget.moc"

