 /***************************************************************************
  *   Copyright (C) 2004 by Riku Leino                                      *
  *   tsoots@gmail.com                                                      *
  *                                                                         *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation; either version 2 of the License, or     *
  *   (at your option) any later version.                                   *
  *                                                                         *
  *   This program is distributed in the hope that it will be useful,       *
  *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
  *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
  *   GNU General Public License for more details.                          *
  *                                                                         *
  *   You should have received a copy of the GNU General Public License     *
  *   along with this program; if not, write to the                         *
  *   Free Software Foundation, Inc.,                                       *
  *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
  ***************************************************************************/
 
 #include "stylereader.h"
 
 #ifdef HAVE_XML
 
 #include <gtmeasure.h>
 #include <gtparagraphstyle.h>
 #include <gtframestyle.h>
 #include <gtfont.h>
 
 StyleReader* StyleReader::sreader = NULL;
 
 extern xmlSAXHandlerPtr sSAXHandler;
 
StyleReader::StyleReader(QString documentName, gtWriter *w,
                         bool textOnly, bool prefix, bool combineStyles)
{
 	sreader      = this;
 	docname      = documentName;
 	readProperties = false;
 	writer       = w;
 	importTextOnly = textOnly;
 	usePrefix    = prefix;
	packStyles   = combineStyles;
 	currentStyle = NULL;
 	parentStyle  = NULL;
 	inList       = false;
 	currentList  = "";
	defaultStyleCreated = false;
}

 bool StyleReader::startElement(const QString&, const QString&, const QString &name, const QXmlAttributes &attrs) 
 {
 	if (name == "style:default-style")
 		defaultStyle(attrs);
 	else if (name == "style:paragraph-properties" ||
 	         name == "style:text-properties" || 
 	         name == "style:list-level-properties")
 		styleProperties(attrs);
 	else if (name == "style:style")
 	{
		if (!defaultStyleCreated)
		{
			currentStyle = new gtParagraphStyle(*(writer->getDefaultStyle()));
			currentStyle->setName("default-style");
			defaultStyleCreated = true;
		}
 		styleStyle(attrs);
 	}
 	else if (name == "style:tab-stop")
 		tabStop(attrs);
 	else if (name == "text:list-style")
 	{
 		for (int i = 0; i < attrs.count(); ++i)
 			if (attrs.localName(i) == "style:name")
 				currentList = attrs.value(i);
 		inList = true;
 	}
 	else if (((name == "text:list-level-style-bullet") ||
 	          (name == "text:list-level-style-number") ||
 	          (name == "text:list-level-style-image")) && (inList))
 	{
 		QString level = "";
 		for (int i = 0; i < attrs.count(); ++i)
 		{
 			if (attrs.localName(i) == "text:level")
 			{
 				gtStyle *plist;
 				if (attrs.value(i) == "1")
 				{
 					plist = listParents[currentList];
 				}
 				else
 				{
 					int level = (attrs.value(i)).toInt();
 					--level;
 					plist = styles[QString(currentList + "_%1").arg(level)]; 
 				}
 				gtParagraphStyle *pstyle;
 				if (plist == NULL)
 					plist = new gtStyle(*(styles["default-style"]));
 
 				if (plist->target() == "paragraph")
 				{
 					pstyle = dynamic_cast<gtParagraphStyle*>(plist);
 					gtParagraphStyle* tmp = new gtParagraphStyle(*pstyle);
 					currentStyle = tmp;
 				}
 				else
 				{
 					gtParagraphStyle* tmp = new gtParagraphStyle(*plist);
 					currentStyle = tmp;
 				}
 				currentStyle->setName(currentList + "_" + attrs.value(i));
 			}
 		}
 		readProperties = true;
 	}
 	else if ((name == "style:drop-cap") && (readProperties))
 	{
 		if (currentStyle->target() == "paragraph")
 		{
 			for (int i = 0; i < attrs.count(); ++i)
 			{
 				if (attrs.localName(i) == "style:lines")
 				{
 					bool ok = false;
 					QString sd = attrs.value(i);
 					int dh = sd.toInt(&ok);
 					if (ok)
 					{
 						gtParagraphStyle* s = dynamic_cast<gtParagraphStyle*>(currentStyle);
 						s->setDropCapHeight(dh);
 						s->setDropCap(true);
 					}
 				}
 			}
 		}
 	}
 	else if (name == "style:font-face")
 	{
 		QString key = "";
 		QString family = "";
 		QString style = "";
 		for (int i = 0; i < attrs.count(); ++i)
 		{
 			if (attrs.localName(i) == "style:name")
 				key = attrs.value(i);
 			else if (attrs.localName(i) == "svg:font-family")
 			{
 				family = attrs.value(i);
 				family = family.remove("'");
 			}
 			else if (attrs.localName(i) == "style:font-style-name")
 				style += attrs.value(i) + " ";
 		}
 		QString name = family + " " + style;
 		name = name.simplifyWhiteSpace();
 		fonts[key] = name;
 	}
 	return true;
 }
 
 void StyleReader::defaultStyle(const QXmlAttributes& attrs)
 {
 	currentStyle = NULL;
 	for (int i = 0; i < attrs.count(); ++i)
 		if (attrs.localName(i) == "style:family")
 			if (attrs.value(i) == "paragraph")
 			{
 				currentStyle = new gtParagraphStyle(*(writer->getDefaultStyle()));
				currentStyle->setName("default-style");
 				readProperties = true;
				defaultStyleCreated = true;
 			}
 }
 
 void StyleReader::styleProperties(const QXmlAttributes& attrs)
 {
 	if ((currentStyle == NULL) || (!readProperties))
 		return;
 	gtParagraphStyle* pstyle = NULL;
 	if (currentStyle->target() == "paragraph")
 		pstyle = dynamic_cast<gtParagraphStyle*>(currentStyle);
 	else
 		pstyle = NULL;
 	QString align = NULL;
 	QString force = NULL;
 	bool hasColorTag = false;
 	for (int i = 0; i < attrs.count(); ++i)
 	{
 		if ((attrs.localName(i) == "style:font-name") && (!inList))
 			currentStyle->getFont()->setName(getFont(attrs.value(i)));
 		else if (attrs.localName(i) == "fo:font-size")
 		{
 			double size = 0;
 			double psize = 0;
 			if (parentStyle != NULL)
 				psize = static_cast<double>(parentStyle->getFont()->getSize());
 			else if (styles.contains("default-style"))
 				psize = static_cast<double>(styles["default-style"]->getFont()->getSize());
 
 			psize = psize / 10;
 			size = getSize(attrs.value(i), psize);
 			int nsize = static_cast<int>(size * 10);
 			currentStyle->getFont()->setSize(nsize);
 			if (pstyle)
 				pstyle->setLineSpacing(writer->getPreferredLineSpacing(nsize));
 		}
 		else if ((attrs.localName(i) == "fo:line-height") && (parentStyle != NULL))
 		{
 			gtParagraphStyle* ppstyle;
 			if (parentStyle->target() == "paragraph")
 			{
 				ppstyle = dynamic_cast<gtParagraphStyle*>(parentStyle);
 				pstyle->setLineSpacing(getSize(attrs.value(i), writer->getPreferredLineSpacing(currentStyle->getFont()->getSize())));
 			}
 		}
 		else if (attrs.localName(i) == "fo:color")
 		{
 			currentStyle->getFont()->setColor(attrs.value(i));
 			hasColorTag = true;
 		}
 		else if ((attrs.localName(i) == "style:use-window-font-color") && (attrs.value(i) == "true"))
 		{
 			currentStyle->getFont()->setColor("Black");
 			hasColorTag = true;
 		}
 		else if ((attrs.localName(i) == "fo:font-weight") && (attrs.value(i) == "bold"))
 			currentStyle->getFont()->setWeight(BOLD);
 		else if ((attrs.localName(i) == "fo:font-style") && (attrs.value(i) == "italic"))
 			currentStyle->getFont()->setSlant(ITALIC);
 		else if ((attrs.localName(i) == "style:text-underline-style") && (attrs.value(i) != "none"))
 			currentStyle->getFont()->toggleEffect(UNDERLINE);
 		else if ((attrs.localName(i) == "style:text-crossing-out") && (attrs.value(i) != "none"))
 			currentStyle->getFont()->toggleEffect(STRIKETHROUGH);
 		else if ((attrs.localName(i) == "fo:font-variant") && (attrs.value(i) == "small-caps"))
 			currentStyle->getFont()->toggleEffect(SMALL_CAPS);
 		else if ((attrs.localName(i) == "style:text-outline") && (attrs.value(i) == "true"))
 		{
 			currentStyle->getFont()->toggleEffect(OUTLINE);
 			currentStyle->getFont()->setStrokeColor("Black");
 			currentStyle->getFont()->setColor("White");
 		}
 		else if (attrs.localName(i) == "fo:letter-spacing")
 			currentStyle->getFont()->setKerning(getSize(attrs.value(i)));
 		else if (attrs.localName(i) == "style:text-scale")
 			currentStyle->getFont()->setHscale(static_cast<int>(getSize(attrs.value(i), -1.0)));
 		else if ((attrs.localName(i) == "style:text-position") && 
 		        (((attrs.value(i)).find("sub") != -1) || 
 				(((attrs.value(i)).left(1) == "-") && ((attrs.value(i)).left(1) != "0"))))
 			currentStyle->getFont()->toggleEffect(SUBSCRIPT);
 		else if ((attrs.localName(i) == "style:text-position") && 
 		        (((attrs.value(i)).find("super") != -1) || 
 				(((attrs.value(i)).left(1) != "-") && ((attrs.value(i)).left(1) != "0"))))
 			currentStyle->getFont()->toggleEffect(SUPERSCRIPT);
 		else if ((attrs.localName(i) == "fo:margin-top") && (pstyle != NULL))
 			pstyle->setSpaceAbove(getSize(attrs.value(i)));
 		else if ((attrs.localName(i) == "fo:margin-bottom") && (pstyle != NULL))
 			pstyle->setSpaceBelow(getSize(attrs.value(i)));
 		else if ((attrs.localName(i) == "fo:margin-left") && (pstyle != NULL))
 		{
 			if (inList)
 				pstyle->setIndent(pstyle->getIndent() + getSize(attrs.value(i)));
 			else
 				pstyle->setIndent(getSize(attrs.value(i)));	
 		}
 		else if ((attrs.localName(i) == "text:space-before") && (pstyle != NULL))
 		{
 			if (inList)
 				pstyle->setIndent(pstyle->getIndent() + getSize(attrs.value(i)));
 			else
 				pstyle->setIndent(getSize(attrs.value(i)));
 		}
 		else if ((attrs.localName(i) == "fo:text-indent") && (pstyle != NULL))
 			pstyle->setFirstLineIndent(getSize(attrs.value(i)));
 		else if ((attrs.localName(i) == "fo:text-align") && (pstyle != NULL))
 			align = attrs.value(i);
 		else if ((attrs.localName(i) == "style:justify-single-word") && (pstyle != NULL))
 			force = attrs.value(i);
 	}
 	if (align != NULL)
 	{
 		if (align == "end")
 			pstyle->setAlignment(RIGHT);
 		else if (align == "center")
 			pstyle->setAlignment(CENTER);
 		else if (align == "justify")
 		{
 			if (force == "false")
 				pstyle->setAlignment(BLOCK);
 			else
 				pstyle->setAlignment(FORCED);
 		}
 	}
	if (!hasColorTag)
		currentStyle->getFont()->setColor("Black");
 }
 
 void StyleReader::styleStyle(const QXmlAttributes& attrs)
 {
 	QString name = "";
 	QString listName = NULL;
 	bool isParaStyle = false;
 	bool create = true;
 	for (int i = 0; i < attrs.count(); ++i)
 	{
 		if (attrs.localName(i) == "style:family")
 		{
 			if (attrs.value(i) == "paragraph")
 			{
 				isParaStyle = true;
 				readProperties = true;
 			}
 			else if (attrs.value(i) == "text")
 			{
 				isParaStyle = false;
 				readProperties = true;
 			}
 			else
 			{
 				readProperties = false;
 				return;
 			}
 		}
 		else if (attrs.localName(i) == "style:name")
 			name = attrs.value(i);
 		else if (attrs.localName(i) == "style:parent-style-name")
 		{
 			if (styles.contains(attrs.value(i)))
 				parentStyle = styles[attrs.value(i)];
 			else
 				parentStyle = NULL;
 		}
 		else if (attrs.localName(i) == "style:list-style-name")
 			listName = attrs.value(i);
 	}
 	if ((parentStyle == NULL) && (styles.contains("default-style")))
 		parentStyle = styles["default-style"];
 
 	if (create)
 	{
 		if (parentStyle == NULL)
 		{
 			parentStyle = new gtStyle("tmp-parent");
 		}
 		if (isParaStyle)
 		{
 			gtParagraphStyle *tmpP;
 			if (parentStyle->target() == "paragraph")
 			{
 				tmpP = dynamic_cast<gtParagraphStyle*>(parentStyle);
 				gtParagraphStyle* tmp = new gtParagraphStyle(*tmpP);
 // 				tmp->setAutoLineSpacing(true);
 				currentStyle = tmp;
 			}
 			else
 			{
 				gtParagraphStyle* tmp = new gtParagraphStyle(*parentStyle);
 // 				tmp->setAutoLineSpacing(true);
 				currentStyle = tmp;
 			}
 			if (listName != NULL)
 			{
 				listParents[listName] = currentStyle;
 			}
 		}
 		else
 			currentStyle = new gtStyle(*parentStyle);
 
 		currentStyle->setName(name);
 	}
 	else
 		currentStyle = NULL;
 }
 
 void StyleReader::tabStop(const QXmlAttributes& attrs)
 {
 	if (currentStyle->target() == "paragraph")
 	{
 		gtParagraphStyle* pstyle = dynamic_cast<gtParagraphStyle*>(currentStyle);
 		QString pos = NULL;
 		QString type = NULL;
 		for (int i = 0; i < attrs.count(); ++i)
 		{
 			if (attrs.localName(i) == "style:position")
 				pos = attrs.value(i);
 			else if (attrs.localName(i) == "style:type")
 				type = attrs.value(i);
 				
 		}
 		if (pos != NULL)
 		{
 			if (type == NULL)
 				type = "left";
 			double posd = getSize(pos);
 			if (type == "left")
 				pstyle->setTabValue(posd, LEFT_T);
 			else if (type == "right")
 				pstyle->setTabValue(posd, RIGHT_T);
 			else if (type == "center")
 				pstyle->setTabValue(posd, CENTER_T);
 			else
 				pstyle->setTabValue(posd, CENTER_T);
 		}
 	}
 }
 
 bool StyleReader::endElement(const QString&, const QString&, const QString &name)
 {
 	if ((name == "style:default-style") && (currentStyle != NULL) && (readProperties))
 	{
 		setStyle(currentStyle->getName(), currentStyle);
 		currentStyle = NULL;
 		parentStyle = NULL;
 		readProperties = false;
 	}
 	else if (((name == "style:style") || 
 	          (name == "text:list-level-style-bullet") || 
 			  (name == "text:list-level-style-number") ||
			  (name == "text:list-level-style-image")) && (currentStyle != NULL))
 	{
 		setStyle(currentStyle->getName(), currentStyle);
 		currentStyle = NULL;
 		parentStyle = NULL;
 		readProperties = false;
 	}
 	else if (name == "text:list-style")
 	{
 		inList = false;
 	}

 	return true;
 }
 
 void StyleReader::parse(QString fileName)
 {
 	xmlSAXParseFile(sSAXHandler, fileName.ascii(), 1);
 }
 
 gtStyle* StyleReader::getStyle(const QString& name)
 {
 	if (styles.contains(name))
 	{
 		gtStyle* tmp = styles[name];
 		QString tname = tmp->getName();
 		if ((tname.find(docname) == -1) && (usePrefix))
 			tmp->setName(docname + "_" + tname);
 
 		return tmp;
 	}
 	else
		return styles["default-style"];

 }
 
 void StyleReader::setStyle(const QString& name, gtStyle* style)
 {
 	gtParagraphStyle *s;
 	QString tname = style->getName();
 	if ((style->target() == "paragraph") && (packStyles))
 	{
 		s = dynamic_cast<gtParagraphStyle*>(style);
 		QString nameByAttrs = QString("%1-").arg(s->getSpaceAbove());
 		nameByAttrs += QString("%1-").arg(s->getSpaceBelow());
 		nameByAttrs += QString("%1-").arg(s->getLineSpacing());
 		nameByAttrs += QString("%1-").arg(s->getIndent());
 		nameByAttrs += QString("%1-").arg(s->getFirstLineIndent());
 		nameByAttrs += QString("%1-").arg(s->getAlignment());
 		nameByAttrs += QString("%1-").arg(s->hasDropCap());
 		nameByAttrs += QString("%1-").arg(s->getFont()->getColor());
 		nameByAttrs += QString("%1-").arg(s->getFont()->getStrokeColor());
 		QValueList<double>* tmp = s->getTabValues();
 		for (uint i = 0; i < tmp->count(); ++i)
 		{
 			double td = (*tmp)[i];
 			nameByAttrs += QString("%1-").arg(td);
 		}
 		if (attrsStyles.contains(nameByAttrs))
 		{
 			tname = attrsStyles[nameByAttrs]->getName();
 			++pstyleCounts[nameByAttrs];
 			style->setName(tname);
 		}
 		else
 		{
 			attrsStyles[nameByAttrs] = style;
 			pstyleCounts[nameByAttrs] = 1;
 			tname = style->getName();
 		}
 	}
 	else if (!packStyles)
 	{
 		attrsStyles[name] = style;
 		pstyleCounts[name] = 1;
 		tname = style->getName();
 	}
 	if (!styles.contains(name))
 	{
 		if ((tname.find(docname) == -1) && (usePrefix))
 			style->setName(docname + "_" + tname);
 		styles[name] = style;
 	}
 }
 
 QString StyleReader::getFont(const QString& key)
 {
 	if (fonts.contains(key))
 		return fonts[key];
 	else
 		return key;
 }
 
 void StyleReader::setupFrameStyle()
 {
 	QString fstyleName = "";
 	int count = 0;
 	CounterMap::Iterator it;
 	for (it = pstyleCounts.begin(); it != pstyleCounts.end(); ++it)
 	{
 		if (it.data() > count)
 		{
 			count = it.data();
 			fstyleName = it.key();
 		}
 	}
 	gtFrameStyle* fstyle;
 	gtParagraphStyle* pstyle = dynamic_cast<gtParagraphStyle*>(attrsStyles[fstyleName]);
 	fstyle = new gtFrameStyle(*pstyle);
 
 	if (!importTextOnly)
 		writer->setFrameStyle(fstyle);
 	delete fstyle;
 }
 
 bool StyleReader::updateStyle(gtStyle* style, gtStyle* parent2Style, const QString& key, const QString& value)
 {
 	gtParagraphStyle* pstyle = NULL;
 	if (style->target() == "paragraph")
 		pstyle = dynamic_cast<gtParagraphStyle*>(style);
 	else
 		pstyle = NULL;
 	QString align = NULL;
 	QString force = NULL;
 
 	if (key == "style:font-name")
 		style->getFont()->setName(getFont(value));
 	else if (key == "fo:font-size")
 	{
 		double size = 0;
 		double psize = 0;
 		if (parent2Style != NULL)
 			psize = static_cast<double>(parent2Style->getFont()->getSize());
 		else if (styles.contains("default-style"))
 			psize = static_cast<double>(styles["default-style"]->getFont()->getSize());
 			psize = psize / 10;
 		size = getSize(value, psize);
 		int nsize = static_cast<int>(size * 10);
 		style->getFont()->setSize(nsize);
 		if (pstyle)
 			pstyle->setLineSpacing(writer->getPreferredLineSpacing(nsize));
 	}
 	else if ((key == "fo:line-height") && (parent2Style != NULL))
 	{
 		gtParagraphStyle* ppstyle;
 		if (parent2Style->target() == "paragraph")
 		{
 			ppstyle = dynamic_cast<gtParagraphStyle*>(parent2Style);
 			pstyle->setLineSpacing(getSize(value, writer->getPreferredLineSpacing(style->getFont()->getSize())));
 		}
 	}
 	else if (key == "fo:color")
		style->getFont()->setColor(value);
	else if ((key == "style:use-window-font-color") && (value == "true"))
 			style->getFont()->setColor("Black");
 	else if ((key == "fo:font-weight") && (value == "bold"))
 		style->getFont()->setWeight(BOLD);
 	else if ((key == "fo:font-style") && (value == "italic"))
 		style->getFont()->setSlant(ITALIC);
 	else if ((key == "style:text-underline-style") && (value != "none"))
 		style->getFont()->toggleEffect(UNDERLINE);
 	else if ((key == "style:text-crossing-out") && (value != "none"))
 		style->getFont()->toggleEffect(STRIKETHROUGH);
 	else if ((key == "fo:font-variant") && (value == "small-caps"))
 		style->getFont()->toggleEffect(SMALL_CAPS);
 	else if ((key == "style:text-outline") && (value == "true"))
 	{
 		style->getFont()->toggleEffect(OUTLINE);
 		style->getFont()->setStrokeColor("Black");
 		style->getFont()->setColor("White");
 	}
 	else if (key == "fo:letter-spacing")
 		style->getFont()->setKerning(getSize(value));
 	else if (key == "style:text-scale")
 		style->getFont()->setHscale(static_cast<int>(getSize(value, -1.0)));
 	else if ((key == "style:text-position") && 
 	        (((value).find("sub") != -1) || 
 			(((value).left(1) == "-") && ((value).left(1) != "0"))))
 		style->getFont()->toggleEffect(SUBSCRIPT);
 	else if ((key == "style:text-position") && 
 	        (((value).find("super") != -1) || 
 			(((value).left(1) != "-") && ((value).left(1) != "0"))))
 		style->getFont()->toggleEffect(SUPERSCRIPT);
 	else if ((key == "fo:margin-top") && (pstyle != NULL))
 		pstyle->setSpaceAbove(getSize(value));
 	else if ((key == "fo:margin-bottom") && (pstyle != NULL))
 		pstyle->setSpaceBelow(getSize(value));
 	else if ((key == "fo:margin-left") && (pstyle != NULL))
 	{
 		if (inList)
 			pstyle->setIndent(pstyle->getIndent() + getSize(value));
 		else
 			pstyle->setIndent(getSize(value));	
 	}
 	else if ((key == "text:space-before") && (pstyle != NULL))
 	{
 		if (inList)
 			pstyle->setIndent(pstyle->getIndent() + getSize(value));
 		else
 			pstyle->setIndent(getSize(value));	
 	}
 	else if ((key == "fo:text-indent") && (pstyle != NULL))
 		pstyle->setFirstLineIndent(getSize(value));
 	else if ((key == "fo:text-align") && (pstyle != NULL))
 		align = value;
 	else if ((key == "style:justify-single-word") && (pstyle != NULL))
 		force = value;
 
 	if (align != NULL)
 	{
 		if (align == "end")
 			pstyle->setAlignment(RIGHT);
 		else if (align == "center")
 			pstyle->setAlignment(CENTER);
 		else if (align == "justify")
 		{
 			if (force != "false")
 				pstyle->setAlignment(FORCED);
 			else
 				pstyle->setAlignment(BLOCK);
 		}
 	}
 	
 	return true;
 }
 
 double StyleReader::getSize(QString s, double parentSize)
 {
 	QString dbl = "0.0";
 	QString lowerValue = s.lower();
 	double ret = 0.0;
 	if (lowerValue.find("pt") != -1)
 	{
 		dbl = lowerValue.remove("pt");
 		ret = gtMeasure::d2d(dbl.toDouble(), PT);
 	}
 	else if (lowerValue.find("mm") != -1)
 	{
 		dbl = lowerValue.remove("mm");
 		ret = gtMeasure::d2d(dbl.toDouble(), MM);
 	}
 	else if (lowerValue.find("cm") != -1)
 	{
 		dbl = lowerValue.remove("cm");
 		ret = gtMeasure::d2d(dbl.toDouble() * 10, MM);
 	}
 	else if (lowerValue.find("in") != -1)
 	{
 		dbl = lowerValue.remove("inch");
 		dbl = lowerValue.remove("in");
 		ret = gtMeasure::d2d(dbl.toDouble(), IN);
 	}
 	else if (lowerValue.find("pi") != -1)
 	{
 		dbl = lowerValue.remove("pica");
 		dbl = lowerValue.remove("pi");
 		ret = gtMeasure::d2d(dbl.toDouble(), P);
 	}
 	else if (lowerValue.find("%") != -1)
 	{
 		dbl = lowerValue.remove("%");
 		double factor = dbl.toDouble();
 		if (parentSize != -1.0)
 		{
 			factor = factor / 100;
 			ret = factor * parentSize;
 		}
 		else
 			ret = factor;
 	}
 	return ret;
 }
 
 StyleReader::~StyleReader()
 {
 	sreader = NULL;
 	StyleMap::Iterator it;
 	for (it = styles.begin(); it != styles.end(); ++it)
 	{
 		if (it.data())
 		{
 			delete it.data();
 			it.data() = NULL;
 		}
 	}
 }
 
 xmlSAXHandler sSAXHandlerStruct = {
 	NULL, // internalSubset,
 	NULL, // isStandalone,
 	NULL, // hasInternalSubset,
 	NULL, // hasExternalSubset,
 	NULL, // resolveEntity,
 	NULL, // getEntity,
 	NULL, // entityDecl,
 	NULL, // notationDecl,
 	NULL, // attributeDecl,
 	NULL, // elementDecl,
 	NULL, // unparsedEntityDecl,
 	NULL, // setDocumentLocator,
 	NULL, // startDocument,
 	NULL, // endDocument,
 	StyleReader::startElement,
 	StyleReader::endElement,
 	NULL, // reference,
 	NULL, // characters
 	NULL, // ignorableWhitespace,
 	NULL, // processingInstruction,
 	NULL, // comment,
 	NULL, // warning,
 	NULL, // error,
 	NULL, // fatalError,
 	NULL, // getParameterEntity,
 	NULL, // cdata,
 	NULL,
 	1
 #ifdef HAVE_XML26
 	,
 	NULL,
 	NULL,
 	NULL,
 	NULL
 #endif
 };
 
 xmlSAXHandlerPtr sSAXHandler = &sSAXHandlerStruct;
 
 void StyleReader::startElement(void*, const xmlChar * fullname, const xmlChar ** atts)
 {
 	QString* name = new QString((const char*) fullname);
 	name = new QString(name->lower());
 	QXmlAttributes* attrs = new QXmlAttributes();
 	if (atts)
 	{
 		for(const xmlChar** cur = atts; cur && *cur; cur += 2)
 			attrs->append(QString((char*)*cur), NULL, QString((char*)*cur), QString((char*)*(cur + 1)));
 	}
 	sreader->startElement(NULL, NULL, *name, *attrs);
 }
 
 void StyleReader::endElement(void*, const xmlChar * name)
 {
 	QString *nname = new QString((const char*) name);
 	nname = new QString(nname->lower());
 	sreader->endElement(NULL, NULL, *nname);
 }
 
 #endif
