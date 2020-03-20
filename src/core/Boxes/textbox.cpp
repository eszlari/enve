// enve - 2D animations software
// Copyright (C) 2016-2020 Maurycy Liebner

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "Boxes/textbox.h"
#include <QInputDialog>
#include <QMenu>
#include "canvas.h"
#include "Animators/gradientpoints.h"
#include "Animators/qstringanimator.h"
#include "RasterEffects/rastereffectcollection.h"
#include "typemenu.h"
#include "Animators/transformanimator.h"
#include "Animators/outlinesettingsanimator.h"
#include "textboxrenderdata.h"
#include "pathboxrenderdata.h"
#include "ReadWrite/evformat.h"
#include "svgexporter.h"

TextBox::TextBox() : PathBox("Text", eBoxType::text) {
    mFillSettings->setPaintType(PaintType::FLATPAINT);
    mFillSettings->setCurrentColor(QColor(0, 0, 0));
    mStrokeSettings->setPaintType(PaintType::NOPAINT);

    const auto pathsUpdater = [this](const UpdateReason reason) {
        setPathsOutdated(reason);
    };

    mText = enve::make_shared<QStringAnimator>("text");
    ca_prependChild(mRasterEffectsAnimators.data(), mText);
    connect(mText.get(), &Property::prp_currentFrameChanged,
            this, pathsUpdater);

    mSpacingCont = enve::make_shared<StaticComplexAnimator>("spacing");
    mLetterSpacing = enve::make_shared<QrealAnimator>(0, -100, 100, 0.1, "letters");
    mWordSpacing = enve::make_shared<QrealAnimator>(1, -100, 100, 0.1, "words");
    mLineSpacing = enve::make_shared<QrealAnimator>(1, -100, 100, 0.1, "lines");

    mSpacingCont->ca_addChild(mLetterSpacing);
    mSpacingCont->ca_addChild(mWordSpacing);
    mSpacingCont->ca_addChild(mLineSpacing);

    ca_prependChild(mRasterEffectsAnimators.data(), mSpacingCont);

    connect(mLetterSpacing.get(), &Property::prp_currentFrameChanged,
            this, pathsUpdater);
    connect(mWordSpacing.get(), &Property::prp_currentFrameChanged,
            this, pathsUpdater);
    connect(mLineSpacing.get(), &Property::prp_currentFrameChanged,
            this, pathsUpdater);

    mTextEffects = enve::make_shared<TextEffectCollection>();
    ca_prependChild(mRasterEffectsAnimators.data(), mTextEffects);
}

#include <QApplication>
#include <QDesktopWidget>

void TextBox::openTextEditor(QWidget* dialogParent) {
    bool ok;
    const QString text =
            QInputDialog::getMultiLineText(
                dialogParent, prp_getName() + " text",
                "Text:", mText->getCurrentValue(), &ok);
    if(ok) {
        mText->prp_startTransform();
        mText->setCurrentValue(text);
        mText->prp_finishTransform();
    }
}

void TextBox::setTextHAlignment(const Qt::Alignment alignment) {
    if(mHAlignment == alignment) return;
    {
        UndoRedo ur;
        const auto oldValue = mHAlignment;
        const auto newValue = alignment;
        ur.fUndo = [this, oldValue]() {
            setTextHAlignment(oldValue);
        };
        ur.fRedo = [this, newValue]() {
            setTextHAlignment(newValue);
        };
        prp_addUndoRedo(ur);
    }
    mHAlignment = alignment;
    setPathsOutdated(UpdateReason::userChange);
}

void TextBox::setTextVAlignment(const Qt::Alignment alignment) {
    if(mVAlignment == alignment) return;
    {
        UndoRedo ur;
        const auto oldValue = mVAlignment;
        const auto newValue = alignment;
        ur.fUndo = [this, oldValue]() {
            setTextVAlignment(oldValue);
        };
        ur.fRedo = [this, newValue]() {
            setTextVAlignment(newValue);
        };
        prp_addUndoRedo(ur);
    }
    mVAlignment = alignment;
    setPathsOutdated(UpdateReason::userChange);
}

void TextBox::setFont(const SkFont &font) {
    if(mFont == font) return;
    {
        UndoRedo ur;
        const auto oldValue = mFont;
        const auto newValue = font;
        ur.fUndo = [this, oldValue]() {
            setFont(oldValue);
        };
        ur.fRedo = [this, newValue]() {
            setFont(newValue);
        };
        prp_addUndoRedo(ur);
    }
    mFont = font;
    prp_afterWholeInfluenceRangeChanged();
    setPathsOutdated(UpdateReason::userChange);
}

void TextBox::setFontSize(const qreal size) {
    setFont(mFont.makeWithSize(size));
}

void TextBox::setFontFamilyAndStyle(const QString &fontFamily,
                                    const SkFontStyle& style) {
    mFamily = fontFamily;
    mStyle = style;
    SkFont newFont = mFont;
    const auto fmlStdStr = fontFamily.toStdString();
    const auto newTypeface = SkTypeface::MakeFromName(fmlStdStr.c_str(), style);
    newFont.setTypeface(newTypeface);
    setFont(newFont);
}

stdsptr<BoxRenderData> TextBox::createRenderData() {
    if(mTextEffects->hasEffects()) {
        return enve::make_shared<TextBoxRenderData>(this);
    } else return PathBox::createRenderData();
}

void TextBox::setupRenderData(const qreal relFrame,
                              BoxRenderData * const data,
                              Canvas * const scene) {
    if(!mTextEffects->hasEffects()) {
        return PathBox::setupRenderData(relFrame, data, scene);
    }
    BoundingBox::setupRenderData(relFrame, data, scene);

    const QString textAtFrame = mText->getValueAtRelFrame(relFrame);

    const qreal letterSpacing = mLetterSpacing->getEffectiveValue(relFrame);
    const qreal wordSpacing = mWordSpacing->getEffectiveValue(relFrame);
    const qreal lineSpacing = mLineSpacing->getEffectiveValue(relFrame);

    const auto textData = static_cast<TextBoxRenderData*>(data);
    textData->initialize(textAtFrame, mFont,
                         letterSpacing, wordSpacing, lineSpacing,
                         mHAlignment, mVAlignment, this, scene);
    QList<TextEffect*> textEffects;
    mTextEffects->addEffects(textEffects);
    for(const auto textEffect : textEffects) {
        textEffect->apply(textData);
    }
    textData->queAllLines();

    if(mCurrentPathsOutdated) {
        mEditPathSk = getRelativePath(anim_getCurrentRelFrame());
        mPathSk = mEditPathSk;
        mFillPathSk = mEditPathSk;

        mCurrentPathsOutdated = false;
    }
}

const SkFontStyle& TextBox::getFontStyle() const {
    return mStyle;
}

SkScalar TextBox::getFontSize() const {
    return mFont.getSize();
}

const QString& TextBox::getFontFamily() const {
    return mFamily;
}

const QString& TextBox::getCurrentValue() const {
    return mText->getCurrentValue();
}

void TextBox::setupCanvasMenu(PropertyMenu * const menu) {
    if(menu->hasActionsForType<TextBox>()) return;
    menu->addedActionsForType<TextBox>();
    PathBox::setupCanvasMenu(menu);
    const auto widget = menu->getParentWidget();
    menu->addSeparator();
    PropertyMenu::PlainSelectedOp<TextBox> txtEff = [](TextBox * box) {
        box->mTextEffects->addChild(enve::make_shared<TextEffect>());
    };
    menu->addPlainAction("Add Text Effect", txtEff);
    PropertyMenu::PlainSelectedOp<TextBox> setText = [widget](TextBox * box) {
        box->openTextEditor(widget);
    };
    menu->addPlainAction("Set Text...", setText);
}
#include "include/core/SkFontMetrics.h"
SkPath TextBox::getRelativePath(const qreal relFrame) const {
    const SkFont font = mFont;
    const qreal fontSize = static_cast<qreal>(font.getSize());
    const QString textAtFrame = mText->getValueAtRelFrame(relFrame);

    const qreal letterSpacing = mLetterSpacing->getEffectiveValue(relFrame);
    const qreal wordSpacing = mWordSpacing->getEffectiveValue(relFrame);
    const qreal lineSpacing = mLineSpacing->getEffectiveValue(relFrame);

    const qreal lineInc = static_cast<qreal>(font.getSpacing())*lineSpacing;

    const QStringList lines = textAtFrame.split(QRegExp("\n|\r\n|\r"));
    qreal maxWidth = 0;
    QList<qreal> lineWidths;
    for(const auto& line : lines) {
        const qreal lineWidth = horizontalAdvance(
                    font, line, letterSpacing, wordSpacing);
        if(lineWidth > maxWidth) maxWidth = lineWidth;
        lineWidths << lineWidth;
    }
    qreal xTranslate;
    if(mHAlignment == Qt::AlignLeft) xTranslate = 0;
    else if(mHAlignment == Qt::AlignRight) xTranslate = -maxWidth;
    else /*if(mHAlignment == Qt::AlignCenter)*/ xTranslate = -0.5*maxWidth;

    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    const qreal height = (lines.count() - 1)*lineInc +
            static_cast<qreal>(metrics.fAscent + metrics.fDescent);
    qreal yTranslate;
    if(mVAlignment == Qt::AlignTop) yTranslate = 0;
    else if(mVAlignment == Qt::AlignBottom) yTranslate = -height;
    else /*if(mVAlignment == Qt::AlignCenter)*/ yTranslate = -0.5*height;

    SkPath result;
    for(int i = 0; i < lines.count(); i++) {
        const auto& line = lines.at(i);
        if(line.isEmpty()) continue;
        const qreal lineWidth = lineWidths.at(i);
        const qreal lineX = textLineX(mHAlignment, lineWidth, maxWidth) + xTranslate;
        const qreal lineY = i*lineInc + yTranslate;
        if(isZero4Dec(letterSpacing) && isOne4Dec(wordSpacing)) {
            SkPath linePath;
            SkTextUtils::GetPath(line.toUtf8().data(),
                                 static_cast<size_t>(line.length()),
                                 SkTextEncoding::kUTF8,
                                 toSkScalar(lineX), toSkScalar(lineY),
                                 font, &linePath);
            result.addPath(linePath);
        } else if(isZero4Dec(letterSpacing)) {
            qreal xPos = lineX;
            const qreal spaceX = horizontalAdvance(font, " ")*wordSpacing;

            const auto wordFinished =
            [&result, &xPos, lineY, &line, &font](const int i0, const int i) {
                const QString wordStr = line.mid(i0, i - i0 + 1);
                SkPath wordPath;
                SkTextUtils::GetPath(wordStr.toUtf8().data(),
                                     static_cast<size_t>(wordStr.length()),
                                     SkTextEncoding::kUTF8,
                                     toSkScalar(xPos), toSkScalar(lineY),
                                     font, &wordPath);
                result.addPath(wordPath);

                xPos += horizontalAdvance(font, wordStr);
            };

            int i0 = 0;
            int nSpaces = 0;
            for(int i = 0; i < line.length(); i++) {
                if(line.at(i) == ' ') {
                    if(nSpaces == 0 && i != 0) wordFinished(i0, i - 1);
                    nSpaces++;
                    i0 = i + 1;
                    xPos += spaceX;
                    continue;
                }
                nSpaces = 0;
            }
            if(i0 < line.length()) wordFinished(i0, line.length() - 1);
        } else {
            qreal xPos = lineX;
            const qreal spaceX = horizontalAdvance(font, " ")*wordSpacing;

            for(int i = 0; i < line.length(); i++) {
                if(line.at(i) == ' ') {
                    xPos += spaceX;
                    continue;
                }
                const QString letter = line.mid(i, 1);
                SkPath letterPath;
                SkTextUtils::GetPath(letter.toUtf8().data(),
                                     static_cast<size_t>(letter.length()),
                                     SkTextEncoding::kUTF8,
                                     toSkScalar(xPos), toSkScalar(lineY),
                                     font, &letterPath);
                result.addPath(letterPath);

                xPos += horizontalAdvance(font, letter) + letterSpacing*fontSize;
            }
        }
    }
    return result;
}

void TextBox::setCurrentValue(const QString &text) {
    mText->setCurrentValue(text);
}

bool TextBox::differenceInEditPathBetweenFrames(
        const int frame1, const int frame2) const {
    if(mText->prp_differencesBetweenRelFrames(frame1, frame2)) return true;
    return mLineSpacing->prp_differencesBetweenRelFrames(frame1, frame2);
}


void TextBox::writeBoundingBox(eWriteStream& dst) const {
    PathBox::writeBoundingBox(dst);
    dst.write(&mHAlignment, sizeof(Qt::Alignment));
    dst.write(&mVAlignment, sizeof(Qt::Alignment));
    dst << qreal(mFont.getSize());
    dst << mFamily;
    dst.write(&mStyle, sizeof(SkFontStyle));
}

void TextBox::readBoundingBox(eReadStream& src) {
    PathBox::readBoundingBox(src);
    src.read(&mHAlignment, sizeof(Qt::Alignment));
    src.read(&mVAlignment, sizeof(Qt::Alignment));
    qreal fontSize;
    QString fontFamily;

    src >> fontSize;
    src >> fontFamily;
    SkFontStyle style;
    if(src.evFileVersion() < EvFormat::textSkFont) {
        QString fontStyle;
        src >> fontStyle;
    } else {
        src.read(&style, sizeof(SkFontStyle));
    }
    mFont.setSize(fontSize);
    setFontFamilyAndStyle(fontFamily, style);
}

void saveTextAttributesSVG(QDomElement& ele,
                           const SkFont& font) {
    ele.setAttribute("font-size", font.getSize());

    SkString familyName;
    QList<SkString> familySet;
    sk_sp<SkTypeface> tface = font.refTypefaceOrDefault();

    SkASSERT(tface);
    SkFontStyle style = tface->fontStyle();
    if (style.slant() == SkFontStyle::kItalic_Slant) {
        ele.setAttribute("font-style", "italic");
    } else if (style.slant() == SkFontStyle::kOblique_Slant) {
        ele.setAttribute("font-style", "oblique");
    }
    int weightIndex = (SkTPin(style.weight(), 100, 900) - 50) / 100;
    if (weightIndex != 3) {
        static constexpr const char* weights[] = {
            "100", "200", "300", "normal", "400", "500", "600", "bold", "800", "900"
        };
        ele.setAttribute("font-weight", weights[weightIndex]);
    }
    int stretchIndex = style.width() - 1;
    if (stretchIndex != 4) {
        static constexpr const char* stretches[] = {
            "ultra-condensed", "extra-condensed", "condensed", "semi-condensed",
            "normal",
            "semi-expanded", "expanded", "extra-expanded", "ultra-expanded"
        };
        ele.setAttribute("font-stretch", stretches[stretchIndex]);
    }

    sk_sp<SkTypeface::LocalizedStrings> familyNameIter(
                tface->createFamilyNameIterator());
    SkTypeface::LocalizedString familyString;
    if (familyNameIter) {
        while (familyNameIter->next(&familyString)) {
            if (familySet.contains(familyString.fString)) {
                continue;
            }
            familySet.append(familyString.fString);
            familyName.appendf((familyName.isEmpty() ? "%s" : ", %s"),
                               familyString.fString.c_str());
        }
    }
    if (!familyName.isEmpty()) {
        ele.setAttribute("font-family", familyName.c_str());
    }
}

QDomElement TextBox::saveSVG(SvgExporter& exp) const {
    auto ele = exp.createElement("g");
    saveTextAttributesSVG(ele, mFont);
    savePathBoxSVG(exp, ele);
    QString textAnchor;
    switch(mHAlignment) {
    case Qt::AlignLeft: textAnchor = "start"; break;
    case Qt::AlignCenter: textAnchor = "middle"; break;
    case Qt::AlignRight: textAnchor = "end"; break;
    }
    ele.setAttribute("text-anchor", textAnchor);
    mText->saveSVG(exp, ele);
    return ele;
}
