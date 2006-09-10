/***************************************************************************
                          brunskatalog.cpp  -
                             -------------------
    begin                : Mon Jul 11 2005
    copyright            : (C) 2003 by Klaas Freitag
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

#ifndef BRUNSKATALOG_H
#define BRUNSKATALOG_H
#include <qintdict.h>
#include <katalog.h>

#include "dbids.h"
#include "brunsrecord.h"

/**
@author Klaas Freitag
*/
typedef QIntDict<QString> KatMap;

class BrunsKatalog : public Katalog
{
public:
    BrunsKatalog( const QString& );

    ~BrunsKatalog();

    virtual int load();

    virtual KatalogType type() {
      return PlantCatalog;
    };
    virtual QStringList getKatalogChapters();
    BrunsRecordList* getRecordList( const QString& chap );
    // virtual void toXML();
    static QStringList formatQuality( BrunsSize& );

    int getEntriesPerChapter( const QString& = QString() ) { return 0; } // FIXME
private:
    void loadDBKeys();
    inline int intPart( const QString& , int , int );

    inline QString toLower( const QString& );
    inline QString toLowerWord( const QString& );

    QString m_chapterFile;
    QString m_dataFile;

    bool m_wantToLower;

    static KatMap m_goods;
    static KatMap m_formAdds;
    static KatMap m_formAddsLong;
    static KatMap m_forms;
    static KatMap m_formsLong;
    static KatMap m_grows;
    static KatMap m_rootPacks;
    static KatMap m_qualities;
    static KatMap m_qualitiesLong;
    static KatMap m_sizeAdds;
    static KatMap m_sizeAddsLong;
    static KatMap m_sizes;

    QIntDict<BrunsRecordList> m_recordLists;
};

#endif
