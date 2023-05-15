/***************************************************************************
             DocumentSaverXML  - Save Documents as XML
                             -------------------
    begin                : Jan. 2021
    copyright            : (C) 2021 by Klaas Freitag
    email                : kraft@freisturz.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// include files for Qt
#include <QDebug>
#include <QSqlQuery>
#include <QDir>
#include <QXmlSchemaValidator>
#include <QXmlSchema>
#include <QDomDocument>
#include <QFileDevice>

#include "models/docbasemodel.h"

#include "documentsaverxml.h"
#include "docposition.h"
#include "docdigest.h"
#include "kraftdoc.h"
#include "unitmanager.h"
#include "defaultprovider.h"
#include "kraftattrib.h"
#include "xmldocindex.h"


namespace {

QString childElemText(const QDomElement& elem, const QByteArray& childName)
{
    const QDomElement e = elem.firstChildElement(childName);
    const QString t = e.text();
    return t;
}

QDate childElemDate(const QDomElement& elem, const QString& childName)
{
    const QDomElement e = elem.firstChildElement(childName);
    const QString t = e.text();
    return QDate::fromString(t, "yyyy-MM-dd");

}

QDomElement xmlTextElement( QDomDocument& doc, const QString& name, const QString& value )
{
  QDomElement elem = doc.createElement( name );
  QDomText t = doc.createTextNode( value.toHtmlEscaped() );
  elem.appendChild( t );
  return elem;
}

int xmlAppendItemsToGroup( QDomDocument& xmldoc, QDomElement& itemGroupElem, KraftDoc *doc)
{
/*
    <type>Normal</type>
     <text>second item </text>
     <amount>4.40</amount>
     <unit>cbm</unit>
     <taxType>reduced</taxType>
     <unitprice>33.33</unitprice>
     <itemprice>146.65</itemprice>
     <tag>Pflanzen</tag>
     <tag>Work</tag>
*/
    int cnt {0};

    for (DocPositionBase *item : doc->positions()) {
        DocPosition *pos = static_cast<DocPosition*>(item);
        QDomElement itemType = xmldoc.createElement("item");

        itemGroupElem.appendChild(itemType);
        QString tStr {"Normal"};
        auto t = item->type();
        if (t == DocPosition::ExtraDiscount) {
            tStr = QStringLiteral("Discount");
            // Only add the type item if the type is not "Normal", which is the default.
            itemType.appendChild(xmlTextElement(xmldoc, "type", tStr));
        }
        itemType.appendChild(xmlTextElement(xmldoc, "text", item->text().toHtmlEscaped()));

        itemType.appendChild(xmlTextElement(xmldoc, "amount", QString::number(pos->amount(), 'f', 2)));
        itemType.appendChild(xmlTextElement(xmldoc, "unit", pos->unit().einheitSingular().toHtmlEscaped()));

        QString ttStr;
        DocPositionBase::TaxType tt = item->taxType();
        // The Full Taxtype is default.
        if (tt == DocPositionBase::TaxType::TaxFull)
            ttStr = QStringLiteral("Full");
        else if (tt == DocPositionBase::TaxType::TaxReduced)
            ttStr = QStringLiteral("Reduced");
        else if (tt == DocPositionBase::TaxType::TaxNone)
            ttStr = QStringLiteral("None");
        else
            ttStr = QStringLiteral("Invalid");

        itemType.appendChild(xmlTextElement(xmldoc, "taxtype", ttStr));

        itemType.appendChild(xmlTextElement(xmldoc, "unitprice", QString::number(pos->unitPrice().toDouble(), 'f', 2)));
        itemType.appendChild(xmlTextElement(xmldoc, "itemprice", QString::number(pos->overallPrice().toDouble(), 'f', 2)));

        const QMap<QString, KraftAttrib> attribs = item->attributes();
        for(const auto &k : attribs.keys()) {
            QDomElement attribElem = xmldoc.createElement("attrib");
            itemType.appendChild(attribElem);
            attribElem.appendChild(xmlTextElement(xmldoc, "name", k.toHtmlEscaped()));
            attribElem.appendChild(xmlTextElement(xmldoc, "value", attribs[k].value().toString().toHtmlEscaped()));
            attribElem.appendChild(xmlTextElement(xmldoc, "type", attribs[k].typeString()));
        }

        cnt++;
    }
    return cnt;
}

QDomDocument xmlDocument(KraftDoc *doc)
{
    QDomDocument xmldoc( "kraftdocument" );
    QDomProcessingInstruction instr = xmldoc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\"");
    xmldoc.appendChild(instr);

    QDomElement root = xmldoc.createElement( "kraftdocument" );
    root.setAttribute("schemaVersion", "1");
    xmldoc.appendChild( root );

    QDomElement meta = xmldoc.createElement( "meta" );
    root.appendChild(meta);
    meta.appendChild(xmlTextElement(xmldoc, "docType", doc->docType()));
    meta.appendChild(xmlTextElement(xmldoc, "docDesc", doc->whiteboard().toHtmlEscaped()));
    meta.appendChild(xmlTextElement(xmldoc, "currency", DefaultProvider::self()->locale()->currencySymbol(QLocale::CurrencyIsoCode).toHtmlEscaped()));
    meta.appendChild(xmlTextElement(xmldoc, "country", DefaultProvider::self()->locale()->countryToString(DefaultProvider::self()->locale()->country()).toHtmlEscaped()));
    meta.appendChild(xmlTextElement(xmldoc, "locale", DefaultProvider::self()->locale()->languageToString(DefaultProvider::self()->locale()->language()).toHtmlEscaped()));
    meta.appendChild(xmlTextElement(xmldoc, "ident", doc->ident() ) );
    meta.appendChild(xmlTextElement(xmldoc, "uuid", doc->uuid()));
    const QDate d = doc->date();
    meta.appendChild(xmlTextElement(xmldoc, "date", d.toString(Qt::ISODate)));
    meta.appendChild(xmlTextElement(xmldoc, "state", doc->stateString()));

    // -------- taxes
    QDomElement taxReduced = xmldoc.createElement("tax");
    meta.appendChild(taxReduced);
    taxReduced.appendChild(xmlTextElement(xmldoc, "type", "Reduced"));
    double t = UnitManager::self()->reducedTax(doc->date());
    taxReduced.appendChild(xmlTextElement(xmldoc, "percent", QString::number(t, 'f', 2)));

    QDomElement taxFull = xmldoc.createElement("tax");
    meta.appendChild(taxFull);
    taxFull.appendChild(xmlTextElement(xmldoc, "type", "Full"));
    t = UnitManager::self()->tax(doc->date());
    taxFull.appendChild(xmlTextElement(xmldoc, "percent", QString::number(t, 'f', 2)));


    // -------- owner etc.
    QString owner = QString::fromLocal8Bit(qgetenv("USER"));
    if (owner.isEmpty())
        owner = QString::fromLocal8Bit(qgetenv("USERNAME"));
    if (!owner.isEmpty()) {
        meta.appendChild(xmlTextElement(xmldoc, "owner", owner));
    }

    QDateTime dt = doc->lastModified();
    if (!dt.isValid())
        dt = QDateTime::currentDateTime();
    meta.appendChild(xmlTextElement(xmldoc, "lastModified", dt.toString(Qt::ISODate)));

    // -------- predecessor
    const QString pred = doc->predecessor();
    if (!pred.isEmpty()) {
        meta.appendChild(xmlTextElement(xmldoc, "predecessor", pred));
    }

    // -------- doc attributes and tags future extensions
    const QMap<QString,KraftAttrib> attrs = doc->attributes();
    for (const auto &attr : attrs.values()) {
        meta.appendChild(attr.toXml(xmldoc));
    }
    for (const QString& tag : doc->allTags()) {
        meta.appendChild(xmlTextElement(xmldoc, "tag", tag));
    }

    // **** Next toplevel: client
    QDomElement cust = xmldoc.createElement( "client" );
    root.appendChild( cust );
    cust.appendChild( xmlTextElement( xmldoc, "address", doc->address() ) );
    cust.appendChild( xmlTextElement( xmldoc, "clientId", doc->addressUid() ) );

    // **** Next toplevel: header
    QDomElement headerElem = xmldoc.createElement( "header" );
    root.appendChild( headerElem );

    const QString prjLabel = doc->projectLabel();
    if (!prjLabel.isEmpty()) {
        QDomElement projectElem = xmldoc.createElement( "project" );
        headerElem.appendChild( projectElem );
        projectElem.appendChild( xmlTextElement(xmldoc, "name", prjLabel));
    }

    // -------- time of supply
    const QDate tosStart = doc->timeOfSupplyStart().date();
    if (tosStart.isValid()) {
        QDate tosEnd   = doc->timeOfSupplyEnd().date();
        QDomElement tos = xmldoc.createElement("timeOfSupply");
        meta.appendChild(tos);
        tos.appendChild(xmlTextElement(xmldoc, "start", tosStart.toString(Qt::ISODate)));
        if (!tosEnd.isValid())
            tosEnd = tosStart;
        tos.appendChild(xmlTextElement(xmldoc, "end", tosEnd.toString(Qt::ISODate)));
    }

    headerElem.appendChild( xmlTextElement( xmldoc, "salut", doc->salut() ) );
    headerElem.appendChild( xmlTextElement( xmldoc, "preText", doc->preText() ) );
    // FIXME: add Header attributs

    // **** Next toplevel: itemGroup, for now only one
    QDomElement itemGroupElem = xmldoc.createElement( "itemGroup" );
    root.appendChild( itemGroupElem );
    itemGroupElem.appendChild( xmlTextElement(xmldoc, "name", QStringLiteral("General")));
    itemGroupElem.appendChild( xmlTextElement(xmldoc, "collapsed", QStringLiteral("false")));

    auto cnt = xmlAppendItemsToGroup(xmldoc, itemGroupElem, doc);
    qDebug() << "Amount of items went to XML" << cnt;

    // **** Next toplevel: footer
    QDomElement itemGroupFooter = xmldoc.createElement("footer");
    root.appendChild(itemGroupFooter);
    itemGroupFooter.appendChild( xmlTextElement( xmldoc, "postText", doc->postText() ) );
    itemGroupFooter.appendChild( xmlTextElement( xmldoc, "goodbye", doc->goodbye() ) );

    // **** Next toplevel: totals
    QDomElement itemGroupTotals = xmldoc.createElement("totals");
    root.appendChild(itemGroupTotals);
    itemGroupTotals.appendChild( xmlTextElement(xmldoc, "netto", QString::number(doc->nettoSum().toDouble(), 'f', 2)));

    QDomElement taxSumReduced = xmldoc.createElement("taxSum");
    itemGroupTotals.appendChild(taxSumReduced);
    taxSumReduced.appendChild(xmlTextElement(xmldoc, "type", "Reduced"));
    t = doc->reducedTaxSum().toDouble();
    taxSumReduced.appendChild(xmlTextElement(xmldoc, "total", QString::number(t, 'f', 2)));

    QDomElement taxSumFull = xmldoc.createElement("taxSum");
    itemGroupTotals.appendChild(taxSumFull);
    taxSumFull.appendChild(xmlTextElement(xmldoc, "type", "Full"));
    t = doc->fullTaxSum().toDouble();
    taxSumFull.appendChild(xmlTextElement(xmldoc, "total", QString::number(t, 'f', 2)));

    itemGroupTotals.appendChild( xmlTextElement(xmldoc, "brutto", QString::number(doc->bruttoSum().toDouble(), 'f', 2)));

    return xmldoc;
}

bool loadMetaBlock(const QDomDocument& domDoc, KraftDoc *doc)
{
    bool res {true};

    QDomElement kraftdocElem = domDoc.firstChildElement("kraftdocument");
    QDomElement metaElem = kraftdocElem.firstChildElement("meta");

    Q_ASSERT(!metaElem.isNull());

    QString t = childElemText(metaElem, "docType");
    doc->setDocType(t);

    t = childElemText(metaElem, "docDesc");
    doc->setWhiteboard(t);

    // Currently the locale of the docs is hardcoded to the locale Kraft is
    // running under. It can not be set into the document.
    t = childElemText(metaElem, "currency");
    t = childElemText(metaElem, "country");
    t = childElemText(metaElem, "locale");

    t = childElemText(metaElem, "ident");
    doc->setIdent(t);

    t = childElemText(metaElem, "uuid");
    doc->setUuid(t);

    QDate d = childElemDate(metaElem, "date");
    doc->setDate(d);

    t = childElemText(metaElem, "state");
    doc->setStateFromString(t);

    // Tax: Unused so far, as tax is taken from documentman,  which loads it according to the date of the doc.
    QDomElement taxElem = metaElem.firstChildElement("tax");
    while (!taxElem.isNull()) {
        t = childElemText(taxElem, "type");
        t = childElemText(taxElem, "value");
        taxElem = taxElem.nextSiblingElement("tax");
    }

    // owner
    t = childElemText(metaElem, "owner");
    doc->setOwner(t);

    // last modified
    t = childElemText(metaElem, "lastModified");
    QDateTime dt = QDateTime::fromString(t, Qt::ISODate);
    doc->setLastModified(dt);

    t = childElemText(metaElem, "predecessor");
    doc->setPredecessor(t);

    QDomElement attrElem = metaElem.firstChildElement("attrib");
    while (!attrElem.isNull()) {
        doc->setAttribute(KraftAttrib(attrElem));
        attrElem = attrElem.nextSiblingElement("attrib");
    }

    // Tags for future fun
    QDomElement tagElem = metaElem.firstChildElement("tag");
    while (!tagElem.isNull()) {
        t = tagElem.text();
        doc->addTag(t);
        tagElem = tagElem.nextSiblingElement("tag");
    }
    return res;
}

bool loadClientBlock(const QDomDocument& domDoc, KraftDoc *doc)
{
    bool res {true};

    QDomElement kraftdocElem = domDoc.firstChildElement("kraftdocument");
    QDomElement clientElem = kraftdocElem.firstChildElement("client");

    Q_ASSERT(!clientElem.isNull());

    const QString t = childElemText(clientElem, "address");
    doc->setAddress(t);
    const QString id = childElemText(clientElem, "clientId");
    doc->setAddressUid(id);

    return res;
}

bool loadHeaderBlock(const QDomDocument& domDoc, KraftDoc *doc)
{
    bool res {true};

    QDomElement kraftdocElem = domDoc.firstChildElement("kraftdocument");
    QDomElement headerElem = kraftdocElem.firstChildElement("header");;

    QDomElement prjElem = headerElem.firstChildElement("project");
    QString t = childElemText(prjElem, "name");
    doc->setProjectLabel(t);
    // TODO: There can also be a project ID

    const QDomElement tosElem = headerElem.firstChildElement("timeOfSupply");
    if (!tosElem.isNull()) {
        const QDate d = childElemDate(tosElem, "start");
        const QDate dEnd = childElemDate(tosElem, "end");
        doc->setTimeOfSupply(QDateTime(d, QTime(0, 0)),
                             QDateTime(dEnd, QTime(23, 59, 59)));
    }

    t = childElemText(headerElem, "salut");
    doc->setSalut(t);

    t = childElemText(headerElem, "preText");
    doc->setPreTextRaw(t);

    QDomElement customElem = headerElem.firstChildElement("attrib");
    while (!customElem.isNull()) {
        t = childElemText(customElem, "name");
        t = childElemText(customElem, "value");
        t = childElemText(customElem, "type");
        customElem = customElem.nextSiblingElement("attrib");
    }

    return res;
}

bool loadItems(const QDomDocument& domDoc, KraftDoc *doc)
{
    bool res {true};

    QDomElement kraftdocElem = domDoc.firstChildElement("kraftdocument");

    // TODO: There should be more than one item Groups
    QDomElement groupElem = kraftdocElem.firstChildElement("itemGroup");
    // TODO: itemGroup name and attribs.

    QDomElement itemElem = groupElem.firstChildElement("item");
    while (!itemElem.isNull()) {
        QString t = childElemText(itemElem, "type");
        DocPositionBase::PositionType itemType {DocPositionBase::Position};
        if (t == "ExtraDiscout") {
            itemType = DocPositionBase::ExtraDiscount;
        }

        DocPosition *item = doc->createPosition(itemType);
        t = childElemText(itemElem, "text");
        item->setText(t);

        t = childElemText(itemElem, "amount");
        double a = t.toDouble();
        item->setAmount(a);
        t = childElemText(itemElem, "unit");
        item->setUnit(UnitManager::self()->getUnit(t));

        t = childElemText(itemElem, "taxType");
        item->setTaxType(t);

        t = childElemText(itemElem, "unitprice");
        item->setUnitPrice(Geld(t.toDouble()));
        t = childElemText(itemElem, "itemprice");

        Geld g(item->overallPrice());
        // qDebug() << "Geld" << g.toLocaleString() << t.toDouble();
        Q_ASSERT(!(g != Geld(t.toDouble())));

        QDomElement attrElem = itemElem.firstChildElement("attrib");
        while (!attrElem.isNull()) {
            const KraftAttrib attr(attrElem);
            item->setAttribute(attr);
            attrElem = attrElem.nextSiblingElement("attrib");
        }

        QDomElement tagElem = itemElem.firstChildElement("tag");
        QStringList tags;
        while (!tagElem.isNull()) {
            tags.append(tagElem.text());
            tagElem = tagElem.nextSiblingElement("tag");
        }
        if (tags.size() > 0) {
            item->setTags(tags);
        }

        // Go to next item
        itemElem = itemElem.nextSiblingElement("item");
    }

    return res;
}

bool loadFooter(const QDomDocument& domDoc, KraftDoc *doc)
{
    bool res {true};

    QDomElement kraftdocElem = domDoc.firstChildElement("kraftdocument");
    QDomElement footerElem = kraftdocElem.firstChildElement("footer");

    QString t = childElemText(footerElem, "postText");
    doc->setPostTextRaw(t);
    t = childElemText(footerElem, "goodbye");
    doc->setGoodbye(t);

    return res;
}

bool loadTotals(const QDomDocument& domDoc, Totals& totals)
{
    bool res {true};

    QDomElement kraftdocElem = domDoc.firstChildElement("kraftdocument");
    QDomElement totalsElem = kraftdocElem.firstChildElement("totals");

    QString t = childElemText(totalsElem, "netto");
    totals._netto = Geld(t.toDouble());
    t = childElemText(totalsElem, "brutto");
    totals._brutto = Geld(t.toDouble());

    QDomElement taxSumElem = totalsElem.firstChildElement("taxSum");
    for( int i = 0; i < 2; i++) {
        t = childElemText(taxSumElem, "type");
        QString sum = childElemText(taxSumElem, "value");
        Geld g(sum.toDouble());
        if (t == "Reduced") {
            totals._redTax = g;
        }
        if (t == "Full") {
            totals._fullTax = g;
        }
        taxSumElem = taxSumElem.nextSiblingElement("taxSum");
    }
    return res;
}

} // namespace end

QString DocumentSaverXML::basePath()
{
    return _basePath.path();
}

void DocumentSaverXML::setBasePath(const QString& path)
{
    _basePath.setPath(path);
}

QString DocumentSaverXML::xmlDocFileName(KraftDoc *doc)
{
    QString path {basePath()};
    const QDate d = doc->date();
    const QString uuid = doc->uuid();
    path.append(QString("/%1/%2/").arg(d.year()).arg(d.month()));

    _basePath.mkpath(path);
    path.append(uuid);
    path.append(".xml");

    return path;
}

QString DocumentSaverXML::xmlDocFileNameFromIdent(const QString& id)
{
    const QString file {id + ".xml"};
    XmlDocIndex indx(basePath());

    const QString p = indx.pathByIdent(id);

    return  p;
}

DocumentSaverXML::DocumentSaverXML()
    : DocumentSaverBase()
{

}

bool DocumentSaverXML::verifyXmlFile(const QUrl& schemaFile, const QString& xmlFile)
{
    QFile file( xmlFile );
    bool re{false};

    QXmlSchema schema;
    if (!schema.load(schemaFile)) {
        qDebug() << "Failed to load schema" << schemaFile.toLocalFile();
    } else {
        if (schema.isValid() && file.open(QIODevice::ReadOnly)) {
            QXmlSchemaValidator validator(schema);
            if (validator.validate(&file, QUrl::fromLocalFile(xmlFile))) {
                re = true;
                qDebug() << "instance document is valid";
            } else {
                qDebug() << "instance document is invalid";
            }
        }
    }
    return re;
}

QString DocumentSaverXML::lastSavedFileName() const
{
    return _lastSaveFile;
}

bool DocumentSaverXML::saveDocument(KraftDoc *doc)
{

    bool result = false;
    if( ! doc ) return result;

    if (!_basePath.exists()) { // not set at all?
        qDebug() << "The base path is not yet set or does not exist!";
        return false;
    }

    QDomDocument xmldoc = xmlDocument(doc);

    if (doc->isNew()) {
        // generate a document ident first.
        qDebug() << "Not yet implemented!";
        return false;
    }

    const QString xml = xmldoc.toString();
    const QString xmlFile = xmlDocFileName(doc);

    qDebug () << "Storing XML to " << xmlFile;

    QFile file( xmlFile );
    if ( file.open( QIODevice::WriteOnly ) ) {
        QTextStream s( &file );
        s << xml << "\n";
        file.close();
    } else {
        qDebug () << "Saving failed";
        return false;
    }

    _lastSaveFile = xmlFile;

    const QUrl schemaFile = QUrl::fromLocalFile(DefaultProvider::self()->locateFile("xml/kraftdoc.xsd"));
    result = verifyXmlFile(schemaFile, xmlFile);

#if 0
    if( !doc->isNew() && doc->docTypeChanged() ) {
        // an existing doc has a new document type. Fix the doc number cycle and pick a new ident
        DocType dt( doc->docType() );
        QString ident = dt.generateDocumentIdent( doc->date(), doc->docType(),
                                                  doc->addressUid() );
        doc->setIdent( ident );
    }

    if( doc->isNew() ) {
        dbID id = KraftDB::self()->getLastInsertID();
        doc->setDocID( id );

        // get the uniq id and write it into the db
        DocType dt( doc->docType() );
        QString ident = dt.generateDocumentIdent( doc->date(), doc->docType(), doc->addressUid() );
        doc->setIdent( ident );

    saveDocumentPositions( doc );
#endif
    // qDebug () << "Saved document no " << doc->docID().toString() << endl;

    return result;
}

bool DocumentSaverXML::loadByIdent(const QString& id, KraftDoc *doc)
{
    if (id.isEmpty()) {
        qDebug() << "Document Id to load is empty!";
    }

    const QString xmlFile = xmlDocFileNameFromIdent(id);

    return loadFromFile(xmlFile, doc);
}

bool DocumentSaverXML::loadFromFile(const QString& xmlFile, KraftDoc *doc, bool onlyMeta)
{
    QFileInfo fi {xmlFile};
    if (!fi.exists()) {
        qDebug() << "File to load does not exist" << xmlFile;
    }

    if (!fi.isReadable()) {
        qDebug() << "File to load not readable" << xmlFile;
    }

    QFile file(xmlFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open xml document file";
        return false;
    }

    QDomDocument _domDoc;

    const QByteArray arr = file.readAll();
    QString errMsg;
    if (!_domDoc.setContent(arr, &errMsg)) {
        qDebug() << "Unable to set file content as xml:" << errMsg;
        file.close();
        return false;
    }
    file.close();

    bool ok;

    ok = loadMetaBlock(_domDoc, doc);
    if (ok) {
        ok = loadClientBlock(_domDoc, doc);
    }

    Totals totals;
    if (!onlyMeta) {
        if (ok) {
            ok = loadHeaderBlock(_domDoc, doc);
        }
        if (ok) {
            ok = loadItems(_domDoc, doc);
        }
        if (ok) {
            ok = loadFooter(_domDoc, doc);
        }
        if (ok) {
            ok = loadTotals(_domDoc, totals);
        }
    }
    return ok;
}

int DocumentSaverXML::addDigestsToModel(DocBaseModel *model)
{
    int cnt{0};
    XmlDocIndex indx(basePath());

    const QMap<QDate, QString> dateMap = indx.dateMap();
    QList<QDate> dates = dateMap.keys();

    std::sort(dates.begin(), dates.end(), [](QDate const& l, QDate const& r) {
        return l < r;
    });

    for( const QDate d : dates) {
        const QString file = dateMap[d];
        KraftDoc doc;
        if (loadFromFile(file, &doc, true)) {
            dbID id; // FIXME remove from digest
            DocDigest digest(id, doc.docType(),
                             doc.addressUid());

            digest.setDate(doc.date());
            digest.setLastModified(doc.lastModified());

            digest.setClientAddress(doc.address());
            digest.setIdent(doc.ident());
            digest.setWhiteboard(doc.whiteboard());
            digest.setProjectLabel(doc.projectLabel());
            model->addData(digest);
            cnt++;
        }

    }

    return cnt;
}
#if 0
    ----

       QSqlQuery q;
        q.prepare("SELECT ident, docType, clientID, clientAddress, salut, goodbye, date, lastModified, language, country, "
                  "pretext, posttext, docDescription, projectlabel, predecessor FROM document WHERE docID=:docID");
        q.bindValue(":docID", id);
        q.exec();
        // qDebug () << "Loading document id " << id << endl;

        if( q.next())
        {
            // qDebug () << "loading document with id " << id << endl;
            dbID dbid;
            dbid = id;
            doc->setDocID(dbid);

            doc->setIdent(      q.value( 0 ).toString() );
            doc->setDocType(    q.value( 1 ).toString() );
            doc->setAddressUid( q.value( 2 ).toString() );
            doc->setAddress(    q.value( 3 ).toString() );
            QString salut = q.value(4).toString();
            doc->setSalut(      salut );
            doc->setGoodbye(    q.value( 5 ).toString() );
            doc->setDate (      q.value( 6 ).toDate() );
            QDateTime dt = q.value(7).toDateTime();

            // Sqlite stores the timestamp as UTC in the database. Mysql does not.
            if (KraftDB::self()->isSqlite()) {
                dt.setTimeSpec(Qt::UTC);
                doc->setLastModified(dt.toLocalTime());
            } else {
                doc->setLastModified(dt);
            }

            // Removed, as with Kraft 0.80 there is no locale management on doc level any more
            // doc->setCountryLanguage( q.value( 8 ).toString(),
            //                         q.value( 9 ).toString());

            doc->setPreText(    KraftDB::self()->mysqlEuroDecode( q.value( 10  ).toString() ) );
            doc->setPostText(   KraftDB::self()->mysqlEuroDecode( q.value( 11 ).toString() ) );
            doc->setWhiteboard( KraftDB::self()->mysqlEuroDecode( q.value( 12 ).toString() ) );
            doc->setProjectLabel( q.value(13).toString() );
            doc->setPredecessor(  q.value(14).toString() );
        }
    }
    // load the dbID of the predecessor document from the database.
    const QString pIdent = doc->predecessor();
    if( ! pIdent.isEmpty() ) {
        QSqlQuery q1;
        q1.prepare("SELECT docID FROM document WHERE ident=:docID");
        q1.bindValue(":docID", pIdent);
        q1.exec();
        if( q1.next() ) {
            const QString pDbId = q1.value(0).toString();
            doc->setPredecessorDbId(pDbId);
        }
    }

    // finally load the item data.
    loadPositions( id, doc );
}
/* docposition:
  +------------+--------------+------+-----+---------+----------------+
  | Field      | Type         | Null | Key | Default | Extra          |
  +------------+--------------+------+-----+---------+----------------+
  | positionID | int(11)      |      | PRI | NULL    | auto_increment |
  | docID      | int(11)      |      | MUL | 0       |                |
  | ordNumber  | int(11)      |      |     | 0       |                |
  | text       | mediumtext   | YES  |     | NULL    |                |
  | amount     | decimal(6,2) | YES  |     | NULL    |                |
  | unit       | varchar(64)  | YES  |     | NULL    |                |
  | price      | decimal(6,2) | YES  |     | NULL    |                |
  +------------+--------------+------+-----+---------+----------------+
*/
void DocumentSaverXML::loadPositions( const QString& id, KraftDoc *doc )
{
    QSqlQuery q;
    q.prepare("SELECT positionID, postype, text, amount, unit, price, taxType FROM docposition WHERE docID=:docID ORDER BY ordNumber");
    q.bindValue(":docID", id);
    q.exec();

    // qDebug () << "* loading document positions for document id " << id << endl;
    while( q.next() ) {
        // qDebug () << " loading position id " << q.value( 0 ).toInt() << endl;

        DocPositionBase::PositionType type = DocPositionBase::Position;
        QString typeStr = q.value( 1 ).toString();
        // if ( typeStr == PosTypeExtraDiscount ) {
        //  type = DocPositionBase::ExtraDiscount;
        // } else if ( typeStr == PosTypePosition ) {
          // nice, default position type.
        //  type = DocPositionBase::Position;
        // } else {
          // qDebug () << "ERROR: Strange type string loaded from db: " << typeStr << endl;
        // }

        DocPosition *dp = doc->createPosition( type );
        dp->setDbId( q.value(0).toInt() );
        dp->setText( q.value(2).toString() );

        // Note: empty fields are treated as Positions which is intended because
        // the type col was added later and thus might be empty for older entries

        dp->setAmount( q.value(3).toDouble() );

        dp->setUnit( UnitManager::self()->getUnit( q.value(4).toInt() ) );
        dp->setUnitPrice( q.value(5).toDouble() );
        dp->setTaxType( q.value(6).toInt() );

        dp->loadAttributes();
    }
#endif

DocumentSaverXML::~DocumentSaverXML( )
{

}

/* END */
