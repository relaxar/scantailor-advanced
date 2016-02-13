/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C)  Joseph Artsimovich <joseph.artsimovich@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "OptionsWidget.h"
#include "ChangeDpiDialog.h"
#include "ChangeDewarpingDialog.h"
#include "ApplyColorsDialog.h"
#include "Settings.h"
#include "PictureZoneComparator.h"
#include "FillZoneComparator.h"
#include "../../Utils.h"
#include "ScopedIncDec.h"
#include <QToolTip>
#include <tiff.h>

namespace output
{

    OptionsWidget::OptionsWidget(
            IntrusivePtr<Settings> const& settings,
            PageSelectionAccessor const& page_selection_accessor)
            : m_ptrSettings(settings),
              m_pageSelectionAccessor(page_selection_accessor),
              m_despeckleLevel(DESPECKLE_NORMAL),
              m_lastTab(TAB_OUTPUT),
              m_ignoreThresholdChanges(0)
    {
        setupUi(this);

        depthPerceptionSlider->setMinimum(qRound(DepthPerception::minValue() * 10));
        depthPerceptionSlider->setMaximum(qRound(DepthPerception::maxValue() * 10));

        colorModeSelector->addItem(tr("Black and White"), ColorParams::BLACK_AND_WHITE);
        colorModeSelector->addItem(tr("Color / Grayscale"), ColorParams::COLOR_GRAYSCALE);
        colorModeSelector->addItem(tr("Mixed"), ColorParams::MIXED);

        pictureShapeSelector->addItem(tr("Free"), FREE_SHAPE);
        pictureShapeSelector->addItem(tr("Rectangular"), RECTANGULAR_SHAPE);
        pictureShapeSelector->addItem(tr("Quadro"), QUADRO_SHAPE);

        tiffCompression->addItem(tr("None"), COMPRESSION_NONE);
        tiffCompression->addItem(tr("LZW"), COMPRESSION_LZW);
        tiffCompression->addItem(tr("Deflate"), COMPRESSION_DEFLATE);
        tiffCompression->addItem(tr("Packbits"), COMPRESSION_PACKBITS);
        tiffCompression->addItem(tr("JPEG"), COMPRESSION_JPEG);

        darkerThresholdLink->setText(
                Utils::richTextForLink(darkerThresholdLink->text())
        );
        lighterThresholdLink->setText(
                Utils::richTextForLink(lighterThresholdLink->text())
        );
        thresholdSlider->setToolTip(QString::number(thresholdSlider->value()));

        updateDpiDisplay();
        updateColorsDisplay();
        updateDewarpingDisplay();

        connect(
                changeDpiButton, SIGNAL(clicked()),
                this, SLOT(changeDpiButtonClicked())
        );
        connect(
                colorModeSelector, SIGNAL(currentIndexChanged(int)),
                this, SLOT(colorModeChanged(int))
        );
        connect(
                pictureShapeSelector, SIGNAL(currentIndexChanged(int)),
                this, SLOT(pictureShapeChanged(int))
        );
        connect(
                tiffCompression, SIGNAL(currentIndexChanged(int)),
                this, SLOT(tiffCompressionChanged(int))
        );
        connect(
                whiteMarginsCB, SIGNAL(clicked(bool)),
                this, SLOT(whiteMarginsToggled(bool))
        );
        connect(
                equalizeIlluminationCB, SIGNAL(clicked(bool)),
                this, SLOT(equalizeIlluminationToggled(bool))
        );
        connect(
                cleanBackgroundCB, SIGNAL(clicked(bool)),
                this, SLOT(cleanBackgroundToggled(bool))
        );

        connect(cleaningAutoBtn, SIGNAL(pressed()), this, SLOT(cleaningAutoMode()));
        connect(cleaningManualBtn, SIGNAL(pressed()), this, SLOT(cleaningManualMode()));

        connect(
                whitenThresholdSlider, SIGNAL(valueChanged(int)),
                this, SLOT(whitenThresholdChanged())
        );
        connect(
                whitenThresholdSlider, SIGNAL(sliderReleased()),
                this, SLOT(whitenThresholdChanged())
        );
        connect(
                brightnessSlider, SIGNAL(valueChanged(int)),
                this, SLOT(brightnessChanged())
        );
        connect(
                brightnessSlider, SIGNAL(sliderReleased()),
                this, SLOT(brightnessChanged())
        );
        connect(
                contrastSlider, SIGNAL(valueChanged(int)),
                this, SLOT(contrastChanged())
        );
        connect(
                contrastSlider, SIGNAL(sliderReleased()),
                this, SLOT(contrastChanged())
        );
        connect(
                lighterThresholdLink, SIGNAL(linkActivated(QString const&)),
                this, SLOT(setLighterThreshold())
        );
        connect(
                darkerThresholdLink, SIGNAL(linkActivated(QString const&)),
                this, SLOT(setDarkerThreshold())
        );
        connect(
                neutralThresholdBtn, SIGNAL(clicked()),
                this, SLOT(setNeutralThreshold())
        );
        connect(
                thresholdSlider, SIGNAL(valueChanged(int)),
                this, SLOT(bwThresholdChanged())
        );
        connect(
                thresholdSlider, SIGNAL(sliderReleased()),
                this, SLOT(bwThresholdChanged())
        );
        connect(
                applyColorsButton, SIGNAL(clicked()),
                this, SLOT(applyColorsButtonClicked())
        );

        connect(
                changeDewarpingButton, SIGNAL(clicked()),
                this, SLOT(changeDewarpingButtonClicked())
        );

        connect(
                applyDepthPerceptionButton, SIGNAL(clicked()),
                this, SLOT(applyDepthPerceptionButtonClicked())
        );

        connect(
                despeckleOffBtn, SIGNAL(clicked()),
                this, SLOT(despeckleOffSelected())
        );
        connect(
                despeckleCautiousBtn, SIGNAL(clicked()),
                this, SLOT(despeckleCautiousSelected())
        );
        connect(
                despeckleNormalBtn, SIGNAL(clicked()),
                this, SLOT(despeckleNormalSelected())
        );
        connect(
                despeckleAggressiveBtn, SIGNAL(clicked()),
                this, SLOT(despeckleAggressiveSelected())
        );
        connect(
                applyDespeckleButton, SIGNAL(clicked()),
                this, SLOT(applyDespeckleButtonClicked())
        );

        connect(
                depthPerceptionSlider, SIGNAL(valueChanged(int)),
                this, SLOT(depthPerceptionChangedSlot(int))
        );

        thresholdSlider->setMinimum(-50);
        thresholdSlider->setMaximum(50);
        thresholLabel->setText(QString::number(thresholdSlider->value()));
    }

    OptionsWidget::~OptionsWidget()
    {
    }

    void
    OptionsWidget::preUpdateUI(PageId const& page_id)
    {
        Params const params(m_ptrSettings->getParams(page_id));
        m_pageId = page_id;
        m_outputDpi = params.outputDpi();
        m_colorParams = params.colorParams();
        m_pictureShape = params.pictureShape();
        m_dewarpingMode = params.dewarpingMode();
        m_depthPerception = params.depthPerception();
        m_despeckleLevel = params.despeckleLevel();

        cleaningAutoBtn->setChecked(true);
        cleaningAutoBtn->setEnabled(false);
        cleaningManualBtn->setEnabled(false);

        updateDpiDisplay();
        updateColorsDisplay();
        updateDewarpingDisplay();
    }

    void
    OptionsWidget::postUpdateUI()
    {
        cleaningAutoBtn->setEnabled(true);
        cleaningManualBtn->setEnabled(true);
    }

    void
    OptionsWidget::tabChanged(ImageViewTab const tab)
    {
        m_lastTab = tab;
        updateDpiDisplay();
        updateColorsDisplay();
        updateDewarpingDisplay();
        reloadIfNecessary();
    }

    void
    OptionsWidget::distortionModelChanged(dewarping::DistortionModel const& model)
    {
        m_ptrSettings->setDistortionModel(m_pageId, model);

        /*if (m_dewarpingMode == DewarpingMode::AUTO)*/ {
            m_ptrSettings->setDewarpingMode(m_pageId, DewarpingMode::MANUAL);
            m_dewarpingMode = DewarpingMode::MANUAL;
            updateDewarpingDisplay();
        }
    }

    void
    OptionsWidget::colorModeChanged(int const idx)
    {
        int const mode = colorModeSelector->itemData(idx).toInt();
        m_colorParams.setColorMode((ColorParams::ColorMode) mode);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);
        updateColorsDisplay();
        emit reloadRequested();
    }

    void
    OptionsWidget::pictureShapeChanged(int const idx)
    {
        m_pictureShape = (PictureShape) (pictureShapeSelector->itemData(idx).toInt());
        m_ptrSettings->setPictureShape(m_pageId, m_pictureShape);
        emit reloadRequested();
    }

    void
    OptionsWidget::tiffCompressionChanged(int idx)
    {
        int compression = tiffCompression->itemData(idx).toInt();
        m_ptrSettings->setTiffCompression(compression);
    }

    void
    OptionsWidget::whiteMarginsToggled(bool const checked)
    {
        ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
        opt.setWhiteMargins(checked);
        if (!checked) {
            opt.setNormalizeIllumination(false);
            equalizeIlluminationCB->setChecked(false);
        }
        m_colorParams.setColorGrayscaleOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);
        equalizeIlluminationCB->setEnabled(checked);
        emit reloadRequested();
    }

    void
    OptionsWidget::equalizeIlluminationToggled(bool const checked)
    {
        ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
        opt.setNormalizeIllumination(checked);
        m_colorParams.setColorGrayscaleOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);
        emit reloadRequested();
    }

    void
    OptionsWidget::cleanBackgroundToggled(bool const checked)
    {
        ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
        opt.setCleanBackground(checked);
        m_colorParams.setColorGrayscaleOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);
        emit reloadRequested();
    }

    void
    OptionsWidget::setLighterThreshold()
    {
        thresholdSlider->setValue(thresholdSlider->value() - 1);
    }

    void
    OptionsWidget::setDarkerThreshold()
    {
        thresholdSlider->setValue(thresholdSlider->value() + 1);
    }

    void
    OptionsWidget::setNeutralThreshold()
    {
        thresholdSlider->setValue(0);
    }

    void
    OptionsWidget::bwThresholdChanged()
    {
        int const value = thresholdSlider->value();
        QString const tooltip_text(QString::number(value));
        thresholdSlider->setToolTip(tooltip_text);

        thresholLabel->setText(QString::number(value));

        if (m_ignoreThresholdChanges) {
            return;
        }

        QPoint const center(thresholdSlider->rect().center());
        QPoint tooltip_pos(thresholdSlider->mapFromGlobal(QCursor::pos()));
        tooltip_pos.setY(center.y());
        tooltip_pos.setX(qBound(0, tooltip_pos.x(), thresholdSlider->width()));
        tooltip_pos = thresholdSlider->mapToGlobal(tooltip_pos);
        QToolTip::showText(tooltip_pos, tooltip_text, thresholdSlider);

        if (thresholdSlider->isSliderDown()) {
            return;
        }

        BlackWhiteOptions opt(m_colorParams.blackWhiteOptions());
        if (opt.thresholdAdjustment() == value) {
            return;
        }

        opt.setThresholdAdjustment(value);
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);
        emit reloadRequested();

        emit invalidateThumbnail(m_pageId);
    }

    void OptionsWidget::cleaningAutoMode()
    {
        ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
        opt.setCleanMode(MODE_AUTO);
        m_colorParams.setColorGrayscaleOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);

        emit reloadRequested();

        emit invalidateThumbnail(m_pageId);
    }

    void OptionsWidget::cleaningManualMode()
    {
        ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
        opt.setCleanMode(MODE_MANUAL);
        m_colorParams.setColorGrayscaleOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    }

    void
    OptionsWidget::whitenThresholdChanged()
    {
        int const value = whitenThresholdSlider->value();
        QString const tooltip_text(QString::number(value));
        whitenThresholdSlider->setToolTip(tooltip_text);

        whitenThresholdLabel->setText(QString::number(value));


        QPoint const center(whitenThresholdSlider->rect().center());
        QPoint tooltip_pos(whitenThresholdSlider->mapFromGlobal(QCursor::pos()));
        tooltip_pos.setY(center.y());
        tooltip_pos.setX(qBound(0, tooltip_pos.x(), whitenThresholdSlider->width()));
        tooltip_pos = whitenThresholdSlider->mapToGlobal(tooltip_pos);
        QToolTip::showText(tooltip_pos, tooltip_text, whitenThresholdSlider);

        if (whitenThresholdSlider->isSliderDown()) {
            return;
        }

        ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
        if (opt.whitenAdjustment() == value) {
            return;
        }

        opt.setWhitenAdjustment(value);
        m_colorParams.setColorGrayscaleOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);

        cleaningManualMode();

        emit reloadRequested();

        emit invalidateThumbnail(m_pageId);
    }

    void
    OptionsWidget::brightnessChanged()
    {
        int const value = brightnessSlider->value();
        QString const tooltip_text(QString::number(value));
        brightnessSlider->setToolTip(tooltip_text);

        brightnessLabel->setText(QString::number(value));


        QPoint const center(brightnessSlider->rect().center());
        QPoint tooltip_pos(brightnessSlider->mapFromGlobal(QCursor::pos()));
        tooltip_pos.setY(center.y());
        tooltip_pos.setX(qBound(0, tooltip_pos.x(), brightnessSlider->width()));
        tooltip_pos = brightnessSlider->mapToGlobal(tooltip_pos);
        QToolTip::showText(tooltip_pos, tooltip_text, brightnessSlider);

        if (brightnessSlider->isSliderDown()) {
            return;
        }

        ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
        if (opt.brightnessAdjustment() == value) {
            return;
        }

        opt.setBrightnessAdjustment(value);
        m_colorParams.setColorGrayscaleOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);

        cleaningManualMode();

        emit reloadRequested();

        emit invalidateThumbnail(m_pageId);
    }

    void
    OptionsWidget::contrastChanged()
    {
        int const value = contrastSlider->value();
        QString const tooltip_text(QString::number(value));
        contrastSlider->setToolTip(tooltip_text);

        contrastLabel->setText(QString::number(value));


        QPoint const center(contrastSlider->rect().center());
        QPoint tooltip_pos(contrastSlider->mapFromGlobal(QCursor::pos()));
        tooltip_pos.setY(center.y());
        tooltip_pos.setX(qBound(0, tooltip_pos.x(), contrastSlider->width()));
        tooltip_pos = contrastSlider->mapToGlobal(tooltip_pos);
        QToolTip::showText(tooltip_pos, tooltip_text, contrastSlider);

        if (contrastSlider->isSliderDown()) {
            return;
        }

        ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
        if (opt.contrastAdjustment() == value) {
            return;
        }

        opt.setContrastAdjustment(value);
        m_colorParams.setColorGrayscaleOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams);

        cleaningManualMode();

        emit reloadRequested();

        emit invalidateThumbnail(m_pageId);
    }

    void
    OptionsWidget::changeDpiButtonClicked()
    {
        ChangeDpiDialog* dialog = new ChangeDpiDialog(
                this, m_outputDpi, m_pageId, m_pageSelectionAccessor
        );
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(
                dialog, SIGNAL(accepted(std::set<PageId> const&, Dpi const&)),
                this, SLOT(dpiChanged(std::set<PageId> const&, Dpi const&))
        );
        dialog->show();
    }

    void
    OptionsWidget::applyColorsButtonClicked()
    {
        ApplyColorsDialog* dialog = new ApplyColorsDialog(
                this, m_pageId, m_pageSelectionAccessor
        );
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(
                dialog, SIGNAL(accepted(std::set<PageId> const&)),
                this, SLOT(applyColorsConfirmed(std::set<PageId> const&))
        );
        dialog->show();
    }

    void
    OptionsWidget::dpiChanged(std::set<PageId> const& pages, Dpi const& dpi)
    {
        for (PageId const& page_id :  pages) {
            m_ptrSettings->setDpi(page_id, dpi);
        }

        if (pages.size() > 1) {
            emit invalidateAllThumbnails();
        }
        else {
            for (PageId const& page_id :  pages) {
                emit invalidateThumbnail(page_id);
            }
        }

        if (pages.find(m_pageId) != pages.end()) {
            m_outputDpi = dpi;
            updateDpiDisplay();
            emit reloadRequested();
        }
    }

    void
    OptionsWidget::applyColorsConfirmed(std::set<PageId> const& pages)
    {
        for (PageId const& page_id :  pages) {
            m_ptrSettings->setColorParams(page_id, m_colorParams);
            m_ptrSettings->setPictureShape(page_id, m_pictureShape);
        }

        if (pages.size() > 1) {
            emit invalidateAllThumbnails();
        }
        else {
            for (PageId const& page_id :  pages) {
                emit invalidateThumbnail(page_id);
            }
        }

        if (pages.find(m_pageId) != pages.end()) {
            emit reloadRequested();
        }
    }

    void
    OptionsWidget::despeckleOffSelected()
    {
        handleDespeckleLevelChange(DESPECKLE_OFF);
    }

    void
    OptionsWidget::despeckleCautiousSelected()
    {
        handleDespeckleLevelChange(DESPECKLE_CAUTIOUS);
    }

    void
    OptionsWidget::despeckleNormalSelected()
    {
        handleDespeckleLevelChange(DESPECKLE_NORMAL);
    }

    void
    OptionsWidget::despeckleAggressiveSelected()
    {
        handleDespeckleLevelChange(DESPECKLE_AGGRESSIVE);
    }

    void
    OptionsWidget::handleDespeckleLevelChange(DespeckleLevel const level)
    {
        m_despeckleLevel = level;
        m_ptrSettings->setDespeckleLevel(m_pageId, level);

        bool handled = false;
        emit despeckleLevelChanged(level, &handled);

        if (handled) {
            emit invalidateThumbnail(m_pageId);
        }
        else {
            emit reloadRequested();
        }
    }

    void
    OptionsWidget::applyDespeckleButtonClicked()
    {
        ApplyColorsDialog* dialog = new ApplyColorsDialog(
                this, m_pageId, m_pageSelectionAccessor
        );
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(tr("Apply Despeckling Level"));
        connect(
                dialog, SIGNAL(accepted(std::set<PageId> const&)),
                this, SLOT(applyDespeckleConfirmed(std::set<PageId> const&))
        );
        dialog->show();
    }

    void
    OptionsWidget::applyDespeckleConfirmed(std::set<PageId> const& pages)
    {
        for (PageId const& page_id :  pages) {
            m_ptrSettings->setDespeckleLevel(page_id, m_despeckleLevel);
        }

        if (pages.size() > 1) {
            emit invalidateAllThumbnails();
        }
        else {
            for (PageId const& page_id :  pages) {
                emit invalidateThumbnail(page_id);
            }
        }

        if (pages.find(m_pageId) != pages.end()) {
            emit reloadRequested();
        }
    }

    void
    OptionsWidget::changeDewarpingButtonClicked()
    {
        ChangeDewarpingDialog* dialog = new ChangeDewarpingDialog(
                this, m_pageId, m_dewarpingMode, m_pageSelectionAccessor
        );
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(
                dialog, SIGNAL(accepted(std::set<PageId> const&, DewarpingMode const&)),
                this, SLOT(dewarpingChanged(std::set<PageId> const&, DewarpingMode const&))
        );
        dialog->show();
    }

    void
    OptionsWidget::dewarpingChanged(std::set<PageId> const& pages, DewarpingMode const& mode)
    {
        for (PageId const& page_id :  pages) {
            m_ptrSettings->setDewarpingMode(page_id, mode);
        }

        if (pages.size() > 1) {
            emit invalidateAllThumbnails();
        }
        else {
            for (PageId const& page_id :  pages) {
                emit invalidateThumbnail(page_id);
            }
        }

        if (pages.find(m_pageId) != pages.end()) {
            if (m_dewarpingMode != mode) {
                m_dewarpingMode = mode;

                if (mode == DewarpingMode::AUTO || m_lastTab != TAB_DEWARPING
                    || mode == DewarpingMode::MARGINAL
                        ) {
                    m_lastTab = TAB_OUTPUT;

                    updateDpiDisplay();
                    updateColorsDisplay();
                    updateDewarpingDisplay();

                    emit reloadRequested();
                }
                else {
                    updateDewarpingDisplay();
                }
            }
        }
    }

    void
    OptionsWidget::applyDepthPerceptionButtonClicked()
    {
        ApplyColorsDialog* dialog = new ApplyColorsDialog(
                this, m_pageId, m_pageSelectionAccessor
        );
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(tr("Apply Depth Perception"));
        connect(
                dialog, SIGNAL(accepted(std::set<PageId> const&)),
                this, SLOT(applyDepthPerceptionConfirmed(std::set<PageId> const&))
        );
        dialog->show();
    }

    void
    OptionsWidget::applyDepthPerceptionConfirmed(std::set<PageId> const& pages)
    {
        for (PageId const& page_id :  pages) {
            m_ptrSettings->setDepthPerception(page_id, m_depthPerception);
        }

        if (pages.size() > 1) {
            emit invalidateAllThumbnails();
        }
        else {
            for (PageId const& page_id :  pages) {
                emit invalidateThumbnail(page_id);
            }
        }

        if (pages.find(m_pageId) != pages.end()) {
            emit reloadRequested();
        }
    }

    void
    OptionsWidget::depthPerceptionChangedSlot(int val)
    {
        m_depthPerception.setValue(0.1 * val);
        QString const tooltip_text(QString::number(m_depthPerception.value()));
        depthPerceptionSlider->setToolTip(tooltip_text);

        QPoint const center(depthPerceptionSlider->rect().center());
        QPoint tooltip_pos(depthPerceptionSlider->mapFromGlobal(QCursor::pos()));
        tooltip_pos.setY(center.y());
        tooltip_pos.setX(qBound(0, tooltip_pos.x(), depthPerceptionSlider->width()));
        tooltip_pos = depthPerceptionSlider->mapToGlobal(tooltip_pos);
        QToolTip::showText(tooltip_pos, tooltip_text, depthPerceptionSlider);

        emit depthPerceptionChanged(m_depthPerception.value());
    }

    void
    OptionsWidget::reloadIfNecessary()
    {
        ZoneSet saved_picture_zones;
        ZoneSet saved_fill_zones;
        DewarpingMode saved_dewarping_mode;
        dewarping::DistortionModel saved_distortion_model;
        DepthPerception saved_depth_perception;
        DespeckleLevel saved_despeckle_level = DESPECKLE_CAUTIOUS;

        std::unique_ptr<OutputParams> output_params(m_ptrSettings->getOutputParams(m_pageId));
        if (output_params.get()) {
            saved_picture_zones = output_params->pictureZones();
            saved_fill_zones = output_params->fillZones();
            saved_dewarping_mode = output_params->outputImageParams().dewarpingMode();
            saved_distortion_model = output_params->outputImageParams().distortionModel();
            saved_depth_perception = output_params->outputImageParams().depthPerception();
            saved_despeckle_level = output_params->outputImageParams().despeckleLevel();
        }

        if (!PictureZoneComparator::equal(saved_picture_zones, m_ptrSettings->pictureZonesForPage(m_pageId))) {
            emit reloadRequested();
            return;
        }
        else if (!FillZoneComparator::equal(saved_fill_zones, m_ptrSettings->fillZonesForPage(m_pageId))) {
            emit reloadRequested();
            return;
        }

        Params const params(m_ptrSettings->getParams(m_pageId));

        if (saved_despeckle_level != params.despeckleLevel()) {
            emit reloadRequested();
            return;
        }

        if (saved_dewarping_mode == DewarpingMode::OFF && params.dewarpingMode() == DewarpingMode::OFF) {
        }
        else if (saved_depth_perception.value() != params.depthPerception().value()) {
            emit reloadRequested();
            return;
        }
        else if (saved_dewarping_mode == DewarpingMode::AUTO && params.dewarpingMode() == DewarpingMode::AUTO) {
        }
        else if (saved_dewarping_mode == DewarpingMode::MARGINAL && params.dewarpingMode() == DewarpingMode::MARGINAL) {
        }
        else if (!saved_distortion_model.matches(params.distortionModel())) {
            emit reloadRequested();
            return;
        }
        else if ((saved_dewarping_mode == DewarpingMode::OFF) != (params.dewarpingMode() == DewarpingMode::OFF)) {
            emit reloadRequested();
            return;
        }
    }

    void
    OptionsWidget::updateDpiDisplay()
    {
        if (m_outputDpi.horizontal() != m_outputDpi.vertical()) {
            dpiLabel->setText(
                    QString::fromLatin1("%1 x %2")
                            .arg(m_outputDpi.horizontal()).arg(m_outputDpi.vertical())
            );
        }
        else {
            dpiLabel->setText(QString::number(m_outputDpi.horizontal()));
        }
    }

    void
    OptionsWidget::updateColorsDisplay()
    {
        colorModeSelector->blockSignals(true);

        ColorParams::ColorMode const color_mode = m_colorParams.colorMode();
        int const color_mode_idx = colorModeSelector->findData(color_mode);
        colorModeSelector->setCurrentIndex(color_mode_idx);

        bool color_grayscale_options_visible = false;
        bool bw_options_visible = false;
        bool picture_shape_visible = false;
        switch (color_mode) {
            case ColorParams::BLACK_AND_WHITE:
                bw_options_visible = true;
                break;
            case ColorParams::COLOR_GRAYSCALE:
                color_grayscale_options_visible = true;
                break;
            case ColorParams::MIXED:
                color_grayscale_options_visible = true;
                bw_options_visible = true;
                picture_shape_visible = true;
                break;
        }

        colorGrayscaleOptions->setVisible(color_grayscale_options_visible);
        if (color_grayscale_options_visible) {
            ColorGrayscaleOptions const opt(
                    m_colorParams.colorGrayscaleOptions()
            );
            if (color_mode != ColorParams::MIXED) {
                whiteMarginsCB->setVisible(true);
                whiteMarginsCB->setChecked(opt.whiteMargins());
                equalizeIlluminationCB->setChecked(opt.normalizeIllumination());
                equalizeIlluminationCB->setEnabled(opt.whiteMargins());
            }
            else {
                whiteMarginsCB->setVisible(false);
                equalizeIlluminationCB->setChecked(opt.normalizeIllumination());
                equalizeIlluminationCB->setEnabled(true);
            }

            cleanBackgroundCB->setChecked(opt.cleanBackground());
            clearBackgroundOptions->setVisible(opt.cleanBackground());
            if (opt.cleanMode() == MODE_AUTO) {
                cleaningAutoBtn->setChecked(true);
            }
            else {
                cleaningManualBtn->setChecked(true);
            }
            brightnessSlider->setValue(opt.brightnessAdjustment());
            contrastSlider->setValue(opt.contrastAdjustment());
            whitenThresholdSlider->setValue(opt.whitenAdjustment());
        }
        else {
            clearBackgroundOptions->setVisible(false);
        }

        modePanel->setVisible(m_lastTab != TAB_DEWARPING);
        pictureShapeOptions->setVisible(picture_shape_visible);
        bwOptions->setVisible(bw_options_visible);
        despecklePanel->setVisible(bw_options_visible && m_lastTab != TAB_DEWARPING);

        if (picture_shape_visible) {
            int const picture_shape_idx = pictureShapeSelector->findData(m_pictureShape);
            pictureShapeSelector->setCurrentIndex(picture_shape_idx);
        }

        int compression_idx = tiffCompression->findData(m_ptrSettings->getTiffCompression());
        tiffCompression->setCurrentIndex(compression_idx);

        if (bw_options_visible) {
            switch (m_despeckleLevel) {
                case DESPECKLE_OFF:
                    despeckleOffBtn->setChecked(true);
                    break;
                case DESPECKLE_CAUTIOUS:
                    despeckleCautiousBtn->setChecked(true);
                    break;
                case DESPECKLE_NORMAL:
                    despeckleNormalBtn->setChecked(true);
                    break;
                case DESPECKLE_AGGRESSIVE:
                    despeckleAggressiveBtn->setChecked(true);
                    break;
            }

            ScopedIncDec<int> const guard(m_ignoreThresholdChanges);
            thresholdSlider->setValue(
                    m_colorParams.blackWhiteOptions().thresholdAdjustment()
            );
        }

        colorModeSelector->blockSignals(false);
    }

    void
    OptionsWidget::updateDewarpingDisplay()
    {
        depthPerceptionPanel->setVisible(m_lastTab == TAB_DEWARPING);

        switch (m_dewarpingMode) {
            case DewarpingMode::OFF:
                dewarpingStatusLabel->setText(tr("Off"));
                break;
            case DewarpingMode::AUTO:
                dewarpingStatusLabel->setText(tr("Auto"));
                break;
            case DewarpingMode::MANUAL:
                dewarpingStatusLabel->setText(tr("Manual"));
                break;
            case DewarpingMode::MARGINAL:
                dewarpingStatusLabel->setText(tr("Marginal"));
                break;
        }

        depthPerceptionSlider->blockSignals(true);
        depthPerceptionSlider->setValue(qRound(m_depthPerception.value() * 10));
        depthPerceptionSlider->blockSignals(false);
    }

}