/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QToolTip>
#include <QDebug>
#include <cmath>
#include "vmshinybar.h"
#include "shinybar.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"

namespace
{
    bool supportsBallooning(const QVariantMap& vmData, XenCache* cache)
    {
        bool isTemplate = vmData.value("is_a_template", false).toBool();
        qint64 dynamicMin = vmData.value("memory_dynamic_min", 0).toLongLong();
        qint64 staticMax = vmData.value("memory_static_max", 0).toLongLong();

        if (isTemplate)
            return dynamicMin != staticMax;

        QString guestMetricsRef = vmData.value("guest_metrics").toString();
        if (guestMetricsRef.isEmpty() || guestMetricsRef == XENOBJECT_NULL || !cache)
            return false;

        QVariantMap guestMetrics = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
        QVariantMap other = guestMetrics.value("other").toMap();
        if (!other.contains("feature-balloon"))
            return false;

        QString value = other.value("feature-balloon").toString().toLower();
        return value == "1" || value == "true" || value == "yes";
    }
}

// Color constants
const QColor VMShinyBar::COLOR_USED = QColor(34, 139, 34);      // ForestGreen
const QColor VMShinyBar::COLOR_TEXT = QColor(255, 255, 255);    // White
const QColor VMShinyBar::COLOR_SLIDER_LIMITS = QColor(211, 211, 211);  // LightGray

VMShinyBar::VMShinyBar(QWidget* parent)
    : ShinyBar(parent),
      memoryUsed_(0),
      staticMin_(0),
      staticMax_(0),
      dynamicMin_(0),
      dynamicMax_(0),
      dynamicMinOrig_(0),
      dynamicMaxOrig_(0),
      hasBallooning_(false),
      allowEdit_(false),
      multiple_(false),
      dynamicMinLowLimit_(0),
      dynamicMinHighLimit_(0),
      dynamicMaxLowLimit_(0),
      dynamicMaxHighLimit_(0),
      increment_(1024 * 1024),  // 1 MB default
      mouseLocation_(-1, -1),
      activeSlider_(Slider::NONE),
      mouseIsDown_(false),
      bytesPerPixel_(0)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // Load slider images
    // TODO: Create actual slider images - for now we'll use colored rectangles
    this->sliderMinImage_ = QPixmap(10, 15);
    this->sliderMaxImage_ = QPixmap(10, 15);
    this->sliderMinImageLight_ = QPixmap(10, 15);
    this->sliderMaxImageLight_ = QPixmap(10, 15);
    this->sliderMinImageDark_ = QPixmap(10, 15);
    this->sliderMaxImageDark_ = QPixmap(10, 15);
    this->sliderMinImageNoEdit_ = QPixmap(10, 15);
    this->sliderMaxImageNoEdit_ = QPixmap(10, 15);
    
    // Fill with basic colors for now
    this->sliderMinImage_.fill(QColor(100, 100, 200));
    this->sliderMaxImage_.fill(QColor(200, 100, 100));
    this->sliderMinImageLight_.fill(QColor(150, 150, 250));
    this->sliderMaxImageLight_.fill(QColor(250, 150, 150));
    this->sliderMinImageDark_.fill(QColor(50, 50, 150));
    this->sliderMaxImageDark_.fill(QColor(150, 50, 50));
    this->sliderMinImageNoEdit_.fill(QColor(100, 100, 100));
    this->sliderMaxImageNoEdit_.fill(QColor(120, 120, 120));
}

void VMShinyBar::Populate(const QList<QSharedPointer<VM>>& vms, bool allowMemEdit)
{
    if (vms.isEmpty())
        return;

    auto vm = vms[0];
    this->multiple_ = vms.count() > 1;
    this->memoryUsed_ = CalcMemoryUsed(vms);
    
    QVariantMap data = vm->GetData();
    this->staticMin_ = data.value("memory_static_min").toLongLong();
    this->staticMax_ = data.value("memory_static_max").toLongLong();
    this->dynamicMin_ = this->dynamicMinOrig_ = data.value("memory_dynamic_min").toDouble();
    this->dynamicMax_ = this->dynamicMaxOrig_ = data.value("memory_dynamic_max").toDouble();
    
    // Check if VM supports ballooning
    this->hasBallooning_ = supportsBallooning(data, vm->GetCache());
    
    this->allowEdit_ = allowMemEdit;
    update();
}

qint64 VMShinyBar::CalcMemoryUsed(const QList<QSharedPointer<VM>>& vms)
{
    QList<qint64> memories;
    
    for (const auto& vm : vms)
    {
        if (!vm)
            continue;
            
        QVariantMap data = vm->GetData();
        QString powerState = data.value("power_state").toString();
        
        if (powerState == "Running" || powerState == "Paused")
        {
            QString metricsRef = data.value("metrics").toString();
            if (!metricsRef.isEmpty())
            {
                // Get metrics from cache
                auto cache = vm->GetCache();
                if (cache)
                {
                    QVariantMap metricsData = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, metricsRef);
                    qint64 memoryActual = metricsData.value("memory_actual").toLongLong();
                    if (memoryActual > 0)
                        memories.append(memoryActual);
                }
            }
        }
    }
    
    if (memories.isEmpty())
        return 0;
        
    // Calculate average
    qint64 sum = 0;
    for (qint64 mem : memories)
        sum += mem;
    return sum / memories.count();
}

void VMShinyBar::SetRanges(double dynamicMinLowLimit, double dynamicMinHighLimit, double dynamicMaxLowLimit, double dynamicMaxHighLimit, const QString& units)
{
    const qint64 BINARY_MEGA = 1024 * 1024;
    const qint64 BINARY_GIGA = 1024 * 1024 * 1024;
    
    if (units == "MB")
    {
        // Round to nearest MB inwards
        double lowMB = std::ceil(dynamicMinLowLimit / BINARY_MEGA);
        double highMB = std::floor(dynamicMinHighLimit / BINARY_MEGA);
        this->dynamicMinLowLimit_ = lowMB * BINARY_MEGA;
        this->dynamicMinHighLimit_ = highMB * BINARY_MEGA;
        
        lowMB = std::ceil(dynamicMaxLowLimit / BINARY_MEGA);
        highMB = std::floor(dynamicMaxHighLimit / BINARY_MEGA);
        this->dynamicMaxLowLimit_ = lowMB * BINARY_MEGA;
        this->dynamicMaxHighLimit_ = highMB * BINARY_MEGA;
    } else
    {
        // Round to nearest GB inwards
        double lowGB = std::ceil(dynamicMinLowLimit / BINARY_GIGA);
        double highGB = std::floor(dynamicMinHighLimit / BINARY_GIGA);
        this->dynamicMinLowLimit_ = lowGB * BINARY_GIGA;
        this->dynamicMinHighLimit_ = highGB * BINARY_GIGA;
        
        lowGB = std::ceil(dynamicMaxLowLimit / BINARY_GIGA);
        highGB = std::floor(dynamicMaxHighLimit / BINARY_GIGA);
        this->dynamicMaxLowLimit_ = lowGB * BINARY_GIGA;
        this->dynamicMaxHighLimit_ = highGB * BINARY_GIGA;
    }
}

void VMShinyBar::ChangeSettings(double staticMin, double dynamicMin, double dynamicMax, double staticMax)
{
    this->staticMin_ = staticMin;
    
    // If we're editing, we never reduce the static_max (really, the "static_max" is just the top
    // of the bar: the real static_max is the position of the top of the range).
    if (!this->allowEdit_ || this->staticMax_ < staticMax)
        this->staticMax_ = staticMax;
    
    // If they're already equal, we don't reset the dynamic_min_orig.
    // (They've probably been set through the sliders not the spinners).
    if (dynamicMin != this->dynamicMin_)
        this->dynamicMin_ = this->dynamicMinOrig_ = dynamicMin;
    if (dynamicMax != this->dynamicMax_)
        this->dynamicMax_ = this->dynamicMaxOrig_ = dynamicMax;
    
    update();
}

void VMShinyBar::SetMemory(Slider slider, double bytes)
{
    bool dragged = false;
    if (slider == Slider::MIN && this->dynamicMin_ != bytes)
    {
        this->dynamicMin_ = bytes;
        dragged = true;
    }
    if (slider == Slider::MAX && this->dynamicMax_ != bytes)
    {
        this->dynamicMax_ = bytes;
        dragged = true;
    }
    if (dragged)
        emit sliderDragged();
}

double VMShinyBar::SliderMinLimit() const
{
    Q_ASSERT(this->activeSlider_ != Slider::NONE);
    return (this->activeSlider_ == Slider::MAX ? this->dynamicMaxLowLimit_ : this->dynamicMinLowLimit_);
}

double VMShinyBar::SliderMaxLimit() const
{
    Q_ASSERT(this->activeSlider_ != Slider::NONE);
    return (this->activeSlider_ == Slider::MIN ? this->dynamicMinHighLimit_ : this->dynamicMaxHighLimit_);
}

QSize VMShinyBar::sizeHint() const
{
    return QSize(600, 80);
}

QSize VMShinyBar::minimumSizeHint() const
{
    return QSize(200, 80);
}

void VMShinyBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    if (this->staticMax_ == 0)  // not initialized
        return;
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect barArea = BarRect();
    this->bytesPerPixel_ = static_cast<double>(this->staticMax_) / static_cast<double>(barArea.width());
    
    // Draw grid
    DrawGrid(painter, barArea, this->bytesPerPixel_, this->staticMax_);
    
    // Draw the memory bar
    int leftWidth = static_cast<int>(static_cast<double>(this->memoryUsed_) / this->bytesPerPixel_);
    if (leftWidth > barArea.width())
        leftWidth = barArea.width();
    
    QRect usedRect(barArea.left(), barArea.top(), leftWidth, barArea.height());
    QString bytesString = Misc::FormatSize(this->memoryUsed_);
    QString toolTip = this->multiple_ ?
        QString("Current memory usage (average): %1").arg(bytesString) :
        QString("Current memory usage: %1").arg(bytesString);
    this->DrawSegment(painter, barArea, usedRect, COLOR_USED, bytesString, COLOR_TEXT, Qt::AlignRight);
    
    QRect unusedRect(barArea.left() + leftWidth, barArea.top(), barArea.width() - leftWidth, barArea.height());
    this->DrawSegment(painter, barArea, unusedRect, COLOR_UNUSED);
    
    // Draw sliders if ballooning is supported
    if (this->hasBallooning_)
    {
        DrawSliderRanges(painter);
        DrawSliders(painter, this->dynamicMin_, this->dynamicMax_);
    }
}

void VMShinyBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!this->allowEdit_)
    {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    this->mouseLocation_ = event->pos();
    
    if (this->activeSlider_ != Slider::NONE)
    {
        double min = SliderMinLimit();
        double max = SliderMaxLimit();
        double orig = (this->activeSlider_ == Slider::MIN ? this->dynamicMinOrig_ : this->dynamicMaxOrig_);
        QRect barArea = BarRect();
        double posBytes = (this->mouseLocation_.x() - barArea.left()) * this->bytesPerPixel_;
        
        if (posBytes <= min)
            posBytes = min;
        else if (posBytes >= max)
            posBytes = max;
        else
        {
            double incrBytes = this->increment_;
            
            // Round to nearest incrBytes
            double roundedBytes = std::round(static_cast<int>((posBytes + incrBytes / 2) / incrBytes) * incrBytes * 10.0) / 10.0;
            
            // We also allow the original value, even if it's not a multiple of
            // incrBytes. That's so that we don't jump as soon as we click it
            // (also so that we can get back to the original value if we want to).
            double distRound = std::abs(posBytes - roundedBytes);
            double distOrig = std::abs(posBytes - orig);
            if (distRound >= distOrig)
                roundedBytes = orig;
            
            // posBytes can fall outside its range before or after the rounding,
            // and both want to be truncated
            if (roundedBytes <= min)
                posBytes = min;
            else if (roundedBytes >= max)
                posBytes = max;
            else
                posBytes = roundedBytes;
        }
        SetMemory(this->activeSlider_, posBytes);
    }
    
    update();
    QWidget::mouseMoveEvent(event);
}

void VMShinyBar::leaveEvent(QEvent* event)
{
    if (this->allowEdit_)
    {
        this->mouseIsDown_ = false;
        this->mouseLocation_ = QPoint(-1, -1);
        this->activeSlider_ = Slider::NONE;
        update();
    }
    
    QWidget::leaveEvent(event);
}

void VMShinyBar::mousePressEvent(QMouseEvent* event)
{
    if (this->allowEdit_ && event->button() == Qt::LeftButton)
    {
        this->mouseIsDown_ = true;
        if (this->minSliderRect_.contains(this->mouseLocation_))
            this->activeSlider_ = Slider::MIN;
        else if (this->maxSliderRect_.contains(this->mouseLocation_))
            this->activeSlider_ = Slider::MAX;
        update();
    }
    
    QWidget::mousePressEvent(event);
}

void VMShinyBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (this->allowEdit_)
    {
        this->mouseIsDown_ = false;
        this->activeSlider_ = Slider::NONE;
        update();
    }
    
    QWidget::mouseReleaseEvent(event);
}

void VMShinyBar::DrawSliderRanges(QPainter& painter)
{
    // Draw slider ranges if we're dragging right now
    if (this->activeSlider_ == Slider::NONE)
        return;
    
    QRect barArea = BarRect();
    int min = barArea.left() + static_cast<int>(SliderMinLimit() / this->bytesPerPixel_);
    int max = barArea.left() + static_cast<int>(SliderMaxLimit() / this->bytesPerPixel_);
    
    painter.fillRect(min, barArea.bottom(), max - min, SLIDER_RANGE_HEIGHT, COLOR_SLIDER_LIMITS);
}

void VMShinyBar::DrawSliders(QPainter& painter, double min, double max)
{
    QRect barArea = BarRect();
    QPixmap minImage, maxImage;
    
    if (this->allowEdit_)
    {
        minImage = this->sliderMinImage_;
        maxImage = this->sliderMaxImage_;
    } else
    {
        minImage = this->sliderMinImageNoEdit_;
        maxImage = this->sliderMaxImageNoEdit_;
    }
    
    // Calculate where to draw the sliders
    QPoint minPt(barArea.left() + static_cast<int>(min / this->bytesPerPixel_) - minImage.width() + (this->allowEdit_ ? 0 : 1),
                 barArea.bottom());
    QPoint maxPt(barArea.left() + static_cast<int>(max / this->bytesPerPixel_) - (this->allowEdit_ ? 0 : 1),
                 barArea.bottom());
    this->minSliderRect_ = QRect(minPt, minImage.size());
    this->maxSliderRect_ = QRect(maxPt, maxImage.size());
    
    // Recalculate the images to draw in case the mouse is over one of them
    if (this->allowEdit_)
    {
        if (this->activeSlider_ == Slider::MIN)
            minImage = this->sliderMinImageDark_;
        if (this->activeSlider_ == Slider::MAX)
            maxImage = this->sliderMaxImageDark_;
        
        if (this->activeSlider_ == Slider::NONE && !this->mouseIsDown_)
        {
            if (this->minSliderRect_.contains(this->mouseLocation_))
                minImage = this->sliderMinImageLight_;
            else if (this->maxSliderRect_.contains(this->mouseLocation_))
                maxImage = this->sliderMaxImageLight_;
        }
    }
    
    // Draw the images
    painter.drawPixmap(minPt, minImage);
    painter.drawPixmap(maxPt, maxImage);
}

void VMShinyBar::DrawGrid(QPainter& painter, const QRect& barArea, double bytesPerPixel, double max)
{
    Q_ASSERT(max > 0);
    
    const int MIN_GAP = 40;  // min gap between consecutive labels
    const int LINE_HEIGHT = 12;
    const qint64 BINARY_MEGA = 1024 * 1024;
    const qint64 BINARY_GIGA = 1024 * 1024 * 1024;
    
    int lineBottom = barArea.top() + barArea.height() / 2;
    int lineTop = barArea.top() - LINE_HEIGHT;
    int textBottom = lineTop - 2;
    
    // Find the size of the longest label
    QString label = QString("%1 MB").arg(static_cast<int>(max / BINARY_MEGA));
    QFontMetrics fm(painter.font());
    int longest = fm.horizontalAdvance(label);
    int textTop = textBottom - fm.height();
    
    // Calculate a suitable increment
    double incr = BINARY_MEGA / 2.0;
    while (incr / bytesPerPixel * 2 < MIN_GAP + longest)
        incr *= 2;
    
    // Draw the grid
    painter.setPen(COLOR_GRID);
    bool withLabel = true;
    for (double x = 0.0; x <= max; x += incr)
    {
        // Tick
        int pos = barArea.left() + static_cast<int>(x / bytesPerPixel);
        painter.drawLine(pos, lineTop, pos, lineBottom);
        
        // Label
        if (withLabel)
        {
            label = Misc::FormatSize(static_cast<qint64>(x));
            QSize size = fm.size(Qt::TextSingleLine, label);
            QRect textRect(QPoint(pos - size.width() / 2, textTop), size);
            
            // Only show labels that are multiples of 0.5 GB if max > 1 GB
            if (max <= BINARY_GIGA || static_cast<qint64>(x) % (BINARY_GIGA / 2) == 0)
            {
                painter.drawText(textRect, Qt::AlignCenter, label);
            }
        }
        withLabel = !withLabel;
    }
}

QRect VMShinyBar::BarRect() const
{
    return QRect(20, 30, width() - 45, BAR_HEIGHT);
}
