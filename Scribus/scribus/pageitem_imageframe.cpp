/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
/***************************************************************************
                          pageitem.cpp  -  description
                             -------------------
    begin                : Sat Apr 7 2001
    copyright            : (C) 2001 by Franz Schmid
    email                : Franz.Schmid@altmuehlnet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// #include <QDebug>
#include <QApplication>
#include <QGridLayout>
#include <QKeyEvent>

#include <cmath>
#include <cassert>

#include "filewatcher.h"
#include "pageitem.h"
#include "pageitem_imageframe.h"
#include "prefsmanager.h"
#include "scraction.h"
#include "scpage.h"
#include "scpaths.h"
#include "scpainter.h"
#include "scribus.h"
#include "scribusstructs.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "commonstrings.h"
#include "undomanager.h"
#include "undostate.h"
#include "scconfig.h"
#include "util_formats.h"
#include "util_color.h"

#include "util.h"


PageItem_ImageFrame::PageItem_ImageFrame(ScribusDoc *pa, double x, double y, double w, double h, double w2, QString fill, QString outline)
	: PageItem(pa, PageItem::ImageFrame, x, y, w, h, w2, fill, outline)
{
}

PageItem_ImageFrame::~PageItem_ImageFrame()
{
	if ((PictureIsAvailable) && (!Pfile.isEmpty()))
	{
		ScCore->fileWatcher->removeFile(Pfile);
		QFileInfo fi(Pfile);
		ScCore->fileWatcher->removeDir(fi.absolutePath());
	}
}

void PageItem_ImageFrame::DrawObj_Item(ScPainter *p, QRectF /*e*/)
{
	if (m_Doc->RePos)
		return;
	if (m_Doc->layerOutline(LayerID))
		return;

	p->setFillRule(true);
	if ((fillColor() != CommonStrings::None) || (GrType != 0))
	{
		p->setupPolygon(&PoLine);
		p->fillPath();
	}
	p->save();
	if (Pfile.isEmpty())
	{
		if ((drawFrame()) && (m_Doc->guidesPrefs().framesShown))
		{
			p->setPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
			p->drawLine(FPoint(0, 0), FPoint(Width, Height));
			p->drawLine(FPoint(0, Height), FPoint(Width, 0));
		}
	}
	else
	{
		//If we are missing our image, draw a red cross in the frame
		if ((!PicArt) || (!PictureIsAvailable))
		{
			if ((drawFrame()) && (m_Doc->guidesPrefs().framesShown))
			{
				p->setBrush(Qt::white);
				QString htmlText = "";
				QFileInfo fi = QFileInfo(Pfile);
				if (PictureIsAvailable)
				{
					p->setPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
					if (isInlineImage)
						htmlText.append( tr("Embedded Image") + "\n");
					else
						htmlText.append( tr("File:") + " " + fi.fileName() + "\n");
					htmlText.append( tr("Original PPI:") + " " + QString::number(qRound(pixm.imgInfo.xres))+" x "+QString::number(qRound(pixm.imgInfo.yres)) + "\n");
					htmlText.append( tr("Actual PPI:") + " " + QString::number(qRound(72.0 / imageXScale()))+" x "+ QString::number(qRound(72.0 / imageYScale())) + "\n");
					htmlText.append( tr("Size:") + " " + QString::number(OrigW) + " x " + QString::number(OrigH) + "\n");
					htmlText.append( tr("Colorspace:") + " ");
					QString cSpace;
					QString ext = fi.suffix().toLower();
					if ((extensionIndicatesPDF(ext) || extensionIndicatesEPSorPS(ext)) && (pixm.imgInfo.type != ImageType7))
						htmlText.append( tr("Unknown"));
					else
						htmlText.append(colorSpaceText(pixm.imgInfo.colorspace));
					if (pixm.imgInfo.numberOfPages > 1)
					{
						htmlText.append("\n");
						if (pixm.imgInfo.actualPageNumber > 0)
							htmlText.append( tr("Page:") + " " + QString::number(pixm.imgInfo.actualPageNumber) + "/" + QString::number(pixm.imgInfo.numberOfPages));
						else
							htmlText.append( tr("Pages:") + " " + QString::number(pixm.imgInfo.numberOfPages));
					}
				}
				else
				{
					p->setPen(Qt::red, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
					htmlText = fi.fileName();
				}
				p->drawLine(FPoint(0, 0), FPoint(Width, Height));
				p->drawLine(FPoint(0, Height), FPoint(Width, 0));
				p->setFont(QApplication::font());
				p->drawText(QRectF(0.0, 0.0, Width, Height), htmlText);
			}
		}
		else
		{
			p->setupPolygon(&PoLine);
			p->setClipPath();
			if (imageFlippedH())
			{
				p->translate(Width, 0);
				p->scale(-1, 1);
			}
			if (imageFlippedV())
			{
				p->translate(0, Height);
				p->scale(1, -1);
			}
			if (imageClip.size() != 0)
			{
				p->setupPolygon(&imageClip);
				p->setClipPath();
			}
			p->translate(m_imageXOffset*m_imageXScale, m_imageYOffset*m_imageYScale);
			p->rotate(m_imageRotation);
			double mscalex = 1.0 / m_imageXScale;
			double mscaley = 1.0 / m_imageYScale;
			p->scale(m_imageXScale, m_imageYScale);
			if (pixm.imgInfo.lowResType != 0)
			{
				p->scale(pixm.imgInfo.lowResScale, pixm.imgInfo.lowResScale);
				mscalex *= 1.0 / pixm.imgInfo.lowResScale;
				mscaley *= 1.0 / pixm.imgInfo.lowResScale;
			}
			if ((GrMask == 1) || (GrMask == 2) || (GrMask == 4) || (GrMask == 5))
			{
				if ((GrMask == 1) || (GrMask == 2))
					p->setMaskMode(1);
				else
					p->setMaskMode(3);
				if ((!gradientMaskVal.isEmpty()) && (!m_Doc->docGradients.contains(gradientMaskVal)))
					gradientMaskVal = "";
				if (!(gradientMaskVal.isEmpty()) && (m_Doc->docGradients.contains(gradientMaskVal)))
					mask_gradient = m_Doc->docGradients[gradientMaskVal];
				p->mask_gradient = mask_gradient;
				if ((GrMask == 1) || (GrMask == 4))
					p->setGradientMask(VGradient::linear, FPoint(GrMaskStartX * mscalex, GrMaskStartY * mscaley), FPoint(GrMaskEndX * mscalex, GrMaskEndY * mscaley), FPoint(GrMaskStartX * mscalex, GrMaskStartY * mscaley), GrMaskScale, GrMaskSkew);
				else
					p->setGradientMask(VGradient::radial, FPoint(GrMaskStartX * mscalex, GrMaskStartY * mscaley), FPoint(GrMaskEndX * mscalex, GrMaskEndY * mscaley), FPoint(GrMaskFocalX * mscalex, GrMaskFocalY * mscaley), GrMaskScale, GrMaskSkew);
			}
			else if ((GrMask == 3) || (GrMask == 6) || (GrMask == 7) || (GrMask == 8))
			{
				if ((patternMaskVal.isEmpty()) || (!m_Doc->docPatterns.contains(patternMaskVal)))
					p->setMaskMode(0);
				else
				{
					p->setPatternMask(&m_Doc->docPatterns[patternMaskVal], patternMaskScaleX * mscalex, patternMaskScaleY * mscaley, patternMaskOffsetX, patternMaskOffsetY, patternMaskRotation, patternMaskSkewX, patternMaskSkewY, patternMaskMirrorX, patternMaskMirrorY);
					if (GrMask == 3)
						p->setMaskMode(2);
					else if (GrMask == 6)
						p->setMaskMode(4);
					else if (GrMask == 7)
						p->setMaskMode(5);
					else
						p->setMaskMode(6);
				}
			}
			else
				p->setMaskMode(0);
			p->drawImage(pixm.qImagePtr());
		}
	}
	p->restore();
}

void PageItem_ImageFrame::clearContents()
{
	if (UndoManager::undoEnabled())
	{
		ScItemState<ScImageEffectList> *is = new ScItemState<ScImageEffectList>(Um::ClearImage + "\n" + Pfile, "");
		is->set("CLEAR_IMAGE", "clear_image");
		is->set("CI_PFILE", Pfile);
		is->set("CI_FLIPPH",imageFlippedH());
		is->set("CI_FLIPPV",imageFlippedV());
		is->set("CI_SCALING",ScaleType);
		is->set("CI_ASPECT",AspectRatio);
		is->set("CI_XOFF",imageXOffset());
		is->set("CI_XSCALE",imageXScale());
		is->set("CI_YOFF",imageYOffset());
		is->set("CI_YSCALE",imageYScale());
		is->set("CI_FILLT", fillTransparency());
		is->set("CI_LINET", lineTransparency());
		is->setItem(effectsInUse);
		undoManager->action(this, is);
	}
	effectsInUse.clear();
	PictureIsAvailable = false;
	Pfile = "";
	pixm = ScImage();

	m_imageXScale = 1;
	m_imageYScale = 1;
	OrigW = 0;
	OrigH = 0;
	m_imageXOffset = 0;
	m_imageYOffset = 0;
	setImageFlippedH(false);
	setImageFlippedV(false);
	EmProfile = "";
	ScaleType = true;
	AspectRatio = false;
	setFillTransparency(0.0);
	setLineTransparency(0.0);
	imageClip.resize(0);
	if (tempImageFile != NULL)
		delete tempImageFile;
	tempImageFile = NULL;
	isInlineImage = false;
	//				emit UpdtObj(Doc->currentPage->pageNr(), ItemNr);
}

void PageItem_ImageFrame::handleModeEditKey(QKeyEvent *k, bool& keyRepeat)
{
	double moveBy=1.0;
	Qt::KeyboardModifiers buttonModifiers = k->modifiers();
	bool controlDown=(buttonModifiers & Qt::ControlModifier);
	bool altDown=(buttonModifiers & Qt::AltModifier);
	bool shiftDown=(buttonModifiers & Qt::ShiftModifier);
	bool resizingImage=false;
	if (shiftDown && !controlDown)
		moveBy=10.0;
	else if (shiftDown && controlDown && !altDown)
		moveBy=0.1;
	else if (shiftDown && controlDown && altDown)
		moveBy=0.01;
	else if (!shiftDown && altDown)
		resizingImage=true;
	double dX=0.0,dY=0.0;
	int kk = k->key();
	if (!resizingImage)
	{
		moveBy/=m_Doc->unitRatio();//Lets allow movement by the current doc ratio, not only points
		switch (kk)
		{
			case Qt::Key_Left:
				dX=-moveBy;
				break;
			case Qt::Key_Right:
				dX=moveBy;
				break;
			case Qt::Key_Up:
				dY=-moveBy;
				break;
			case Qt::Key_Down:
				dY=moveBy;
				break;
		}
		if (dX!=0.0 || dY!=0.0)
		{
			moveImageInFrame(dX, dY);
			update();
		}
	}
	else
	{
		switch (kk)
		{
			case Qt::Key_Left:
				dX=-moveBy+100;
				break;
			case Qt::Key_Right:
				dX=moveBy+100;
				break;
			case Qt::Key_Up:
				dY=-moveBy+100;
				break;
			case Qt::Key_Down:
				dY=moveBy+100;
				break;
			default:
				return;
		}		
		if (dX!=0.0)
		{
			double newXScale=dX / 100.0 * m_imageXScale;
			setImageXScale(newXScale);
			if (!controlDown)
			{
				double newYScale=dX / 100.0 * m_imageYScale;
				setImageYScale(newYScale);
			}
		}
		else
		if (dY!=0.0)
		{
			double newYScale=dY / 100.0 * m_imageYScale;
			setImageYScale(newYScale);
			if (!controlDown)
			{
				double newXScale=dY / 100.0 * m_imageYScale;
				setImageXScale(newXScale);
			}
		}
		if (dX!=0.0 || dY!=0.0)
			if (imageClip.size() != 0)
			{
				imageClip = pixm.imgInfo.PDSpathData[pixm.imgInfo.usedPath].copy();
				QTransform cl;
				cl.translate(imageXOffset()*imageXScale(), imageYOffset()*imageYScale());
				cl.rotate(imageRotation());
				cl.scale(imageXScale(), imageYScale());
				imageClip.map(cl);
			}
		update();	
	}
}

bool PageItem_ImageFrame::createInfoGroup(QFrame *infoGroup, QGridLayout *infoGroupLayout)
{
	QLabel *infoCT, *fileT, *fileCT, *oPpiT, *oPpiCT, *aPpiT, *aPpiCT, *colT, *colCT;
	infoCT = new QLabel(infoGroup);
	fileCT = new QLabel(infoGroup);
	
	infoCT->setText( tr("Image"));
	infoGroupLayout->addWidget( infoCT, 0, 0, 1, 2, Qt::AlignHCenter );
	
	if (PictureIsAvailable)
	{
		fileT = new QLabel(infoGroup);
		oPpiT = new QLabel(infoGroup);
		oPpiCT = new QLabel(infoGroup);
		aPpiT = new QLabel(infoGroup);
		aPpiCT = new QLabel(infoGroup);
		colT = new QLabel(infoGroup);
		colCT = new QLabel(infoGroup);
		QFileInfo fi = QFileInfo(Pfile);
		fileCT->setText( tr("File:"));
		infoGroupLayout->addWidget( fileCT, 1, 0, Qt::AlignRight );
		if (isInlineImage)
			fileT->setText( tr("Embedded Image"));
		else
			fileT->setText(fi.fileName());
		infoGroupLayout->addWidget( fileT, 1, 1 );
		
		oPpiCT->setText( tr("Original PPI:"));
		infoGroupLayout->addWidget( oPpiCT, 2, 0, Qt::AlignRight );
		oPpiT->setText(QString::number(qRound(pixm.imgInfo.xres))+" x "+QString::number(qRound(pixm.imgInfo.yres)));
		infoGroupLayout->addWidget( oPpiT, 2, 1 );
		
		aPpiCT->setText( tr("Actual PPI:"));
		infoGroupLayout->addWidget( aPpiCT, 3, 0, Qt::AlignRight );
		aPpiT->setText(QString::number(qRound(72.0 / imageXScale()))+" x "+ QString::number(qRound(72.0 / imageYScale())));
		infoGroupLayout->addWidget( aPpiT, 3, 1 );
		
		colCT->setText( tr("Colorspace:"));
		infoGroupLayout->addWidget( colCT, 4, 0, Qt::AlignRight );
		QString cSpace;
		QString ext = fi.suffix().toLower();
		if ((extensionIndicatesPDF(ext) || extensionIndicatesEPSorPS(ext)) && (pixm.imgInfo.type != ImageType7))
			cSpace = tr("Unknown");
		else
			cSpace=colorSpaceText(pixm.imgInfo.colorspace);
		colT->setText(cSpace);
		infoGroupLayout->addWidget( colT, 4, 1 );
	}
	else
	{
		if (!Pfile.isEmpty())
		{
			QFileInfo fi = QFileInfo(Pfile);
			fileCT->setText( tr("File:"));
			infoGroupLayout->addWidget( fileCT, 1, 0, Qt::AlignRight );
			fileT = new QLabel(infoGroup);
			if (isInlineImage)
				fileT->setText( tr("Embedded Image missing"));
			else if (extensionIndicatesPDF(fi.suffix().toLower()))
				fileT->setText(fi.fileName() + " " + tr("missing or corrupt"));
			else
				fileT->setText(fi.fileName() + " " + tr("missing"));
			infoGroupLayout->addWidget( fileT, 1, 1 );
		}
		else
		{
			fileCT->setText( tr("No Image Loaded"));
			infoGroupLayout->addWidget( fileCT, 1, 0, 1, 2, Qt::AlignHCenter );
		}
	}
	return true;
}

void PageItem_ImageFrame::applicableActions(QStringList & actionList)
{
	actionList << "fileImportImage";
	actionList << "itemConvertToTextFrame";
	actionList << "itemImageIsVisible";
	actionList << "itemPreviewFull";
	actionList << "itemPreviewLow";
	actionList << "itemPreviewNormal";

	if (PictureIsAvailable)
	{
		actionList << "itemAdjustFrameToImage";
		actionList << "itemAdjustImageToFrame";
		if (pixm.imgInfo.valid)
			actionList << "itemExtendedImageProperties";
		if (pixm.imgInfo.exifDataValid)
			actionList << "itemImageInfo";
		actionList << "itemUpdateImage";
		actionList << "editCopyContents";
		actionList << "itemToggleInlineImage";
		if (isRaster)
		{
			actionList << "styleImageEffects";
			actionList << "editEditWithImageEditor";
		}
	}
	if(!Pfile.isEmpty())
		actionList << "editClearContents";
	actionList << "itemConvertToPolygon";
	if (doc()->scMW()->contentsBuffer.sourceType==PageItem::ImageFrame)
	{
		actionList << "editPasteContents";
		actionList << "editPasteContentsAbs";
	}
}

QString PageItem_ImageFrame::infoDescription()
{
	QString htmlText;
	htmlText.append( tr("Image") + "<br/>");
	
	if (PictureIsAvailable)
	{
		QFileInfo fi = QFileInfo(Pfile);
		if (isInlineImage)
			htmlText.append( tr("Embedded Image") + "<br/>");
		else
			htmlText.append( tr("File:") + " " + fi.fileName() + "<br/>");
		htmlText.append( tr("Original PPI:") + " " + QString::number(qRound(pixm.imgInfo.xres))+" x "+QString::number(qRound(pixm.imgInfo.yres)) + "<br/>");
		htmlText.append( tr("Actual PPI:") + " " + QString::number(qRound(72.0 / imageXScale()))+" x "+ QString::number(qRound(72.0 / imageYScale())) + "<br/>");
		htmlText.append( tr("Colorspace:") + " ");
		QString cSpace;
		QString ext = fi.suffix().toLower();
		if ((extensionIndicatesPDF(ext) || extensionIndicatesEPSorPS(ext)) && (pixm.imgInfo.type != ImageType7))
			htmlText.append( tr("Unknown"));
		else
			htmlText.append(colorSpaceText(pixm.imgInfo.colorspace));
		htmlText.append("<br/>");
		if (pixm.imgInfo.numberOfPages > 1)
		{
			if (pixm.imgInfo.actualPageNumber > 0)
				htmlText.append( tr("Page:") + " " + QString::number(pixm.imgInfo.actualPageNumber) + "/" + QString::number(pixm.imgInfo.numberOfPages)+ "<br/>");
			else
				htmlText.append( tr("Pages:") + " " + QString::number(pixm.imgInfo.numberOfPages)+ "<br/>");
		}
	}
	else
	{
		if (!Pfile.isEmpty())
		{
			QFileInfo fi = QFileInfo(Pfile);
			if (isInlineImage)
				htmlText.append( tr("Embedded Image missing") + "<br/>");
			else if (extensionIndicatesPDF(fi.suffix().toLower()))
				htmlText.append( tr("File:") + " " + fi.fileName() + " " + tr("missing or corrupt") + "<br/>");
			else
				htmlText.append( tr("File:") + " " + fi.fileName() + " " + tr("missing") + "<br/>");
		}
		else
			htmlText.append( tr("No Image Loaded") + "<br/>");
	}
	htmlText.append(PageItem::infoDescription());
	return htmlText;
}
