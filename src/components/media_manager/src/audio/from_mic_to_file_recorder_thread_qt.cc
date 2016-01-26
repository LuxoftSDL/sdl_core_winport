/*
 * Copyright (c) 2016, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
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
#include "media_manager/audio/from_mic_to_file_recorder_thread.h"
#include <QtMultimedia/QMediaRecorder>
#include <QtMultimedia/QAudioProbe>
#include <QtMultimedia/QAudioRecorder>
#include <ostream>

#include "utils/threads/thread.h"
#include "utils/logger.h"

namespace media_manager {

CREATE_LOGGERPTR_GLOBAL(logger_, "FromMicToFileRecorderThread")

template <class T>
static QVector<qreal> getBufferLevels(const T* buffer,
                                      int frames,
                                      int channels) {
  QVector<qreal> max_values;
  max_values.fill(0, channels);

  for (int i = 0; i < frames; ++i) {
    for (int j = 0; j < channels; ++j) {
      qreal value = qAbs(qreal(buffer[i * channels + j]));
      if (value > max_values.at(j))
        max_values.replace(j, value);
    }
  }

  return max_values;
}

static qreal getPeakValue(const QAudioFormat& format) {
  // Note: Only the most common sample formats are supported
  if (!format.isValid())
    return qreal(0);

  if (format.codec() != "audio/pcm")
    return qreal(0);

  switch (format.sampleType()) {
    case QAudioFormat::Unknown:
      break;
    case QAudioFormat::Float:
      if (format.sampleSize() != 32)  // other sample formats are not supported
        return qreal(0);
      return qreal(1.00003);
    case QAudioFormat::SignedInt:
      if (format.sampleSize() == 32)
        return qreal(INT_MAX);
      if (format.sampleSize() == 16)
        return qreal(SHRT_MAX);
      if (format.sampleSize() == 8)
        return qreal(CHAR_MAX);
      break;
    case QAudioFormat::UnSignedInt:
      if (format.sampleSize() == 32)
        return qreal(UINT_MAX);
      if (format.sampleSize() == 16)
        return qreal(USHRT_MAX);
      if (format.sampleSize() == 8)
        return qreal(UCHAR_MAX);
      break;
  }

  return qreal(0);
}

static QVector<qreal> getBufferLevels(const QAudioBuffer& buffer) {
  QVector<qreal> values;

  if (!buffer.format().isValid() ||
      buffer.format().byteOrder() != QAudioFormat::LittleEndian)
    return values;

  if (buffer.format().codec() != "audio/pcm")
    return values;

  int channelCount = buffer.format().channelCount();
  values.fill(0, channelCount);
  qreal peak_value = getPeakValue(buffer.format());
  if (qFuzzyCompare(peak_value, qreal(0)))
    return values;

  switch (buffer.format().sampleType()) {
    case QAudioFormat::Unknown:
    case QAudioFormat::UnSignedInt:
      if (buffer.format().sampleSize() == 32)
        values = getBufferLevels(
            buffer.constData<quint32>(), buffer.frameCount(), channelCount);
      if (buffer.format().sampleSize() == 16)
        values = getBufferLevels(
            buffer.constData<quint16>(), buffer.frameCount(), channelCount);
      if (buffer.format().sampleSize() == 8)
        values = getBufferLevels(
            buffer.constData<quint8>(), buffer.frameCount(), channelCount);
      for (int i = 0; i < values.size(); ++i)
        values[i] = qAbs(values.at(i) - peak_value / 2) / (peak_value / 2);
      break;
    case QAudioFormat::Float:
      if (buffer.format().sampleSize() == 32) {
        values = getBufferLevels(
            buffer.constData<float>(), buffer.frameCount(), channelCount);
        for (int i = 0; i < values.size(); ++i)
          values[i] /= peak_value;
      }
      break;
    case QAudioFormat::SignedInt:
      if (buffer.format().sampleSize() == 32)
        values = getBufferLevels(
            buffer.constData<qint32>(), buffer.frameCount(), channelCount);
      if (buffer.format().sampleSize() == 16)
        values = getBufferLevels(
            buffer.constData<qint16>(), buffer.frameCount(), channelCount);
      if (buffer.format().sampleSize() == 8)
        values = getBufferLevels(
            buffer.constData<qint8>(), buffer.frameCount(), channelCount);
      for (int i = 0; i < values.size(); ++i)
        values[i] /= peak_value;
      break;
  }
  return values;
}

////////////////////////////////////////////////////////////////////////////////
/// media_manager::FromMicToFileRecorderThread::Impl
////////////////////////////////////////////////////////////////////////////////

class FromMicToFileRecorderThread::Impl : public QObject {
  Q_OBJECT
 public:
  Impl(const std::string& container);
  ~Impl();

  void startRecord();
  void stopRecord();
  const std::string getDeviceName() const;

  Q_SLOT void processBuffer(const QAudioBuffer&);

  int32_t getDuration() const;
  void setDuration(const int32_t duration);
  void setShouldBeStoped(const bool shouldBeStoped);

 private slots:
  void updateProgress(qint64 pos);
  void displayErrorMessage();

 private:
  QString device_;
  QString codec_;
  QString container_type_;

  int32_t duration_;
  bool shouldBeStoped_;
  sync_primitives::Lock stopFlagLock_;
  sync_primitives::Lock flagDuration_;

 private:
  QAudioEncoderSettings settings_;
  QAudioRecorder* audioRecorder_;
  QAudioProbe* audioProbe_;
  DISALLOW_COPY_AND_ASSIGN(Impl);
};
}  // namespace media_manager

media_manager::FromMicToFileRecorderThread::Impl::Impl(
    const std::string& container)
    : settings_()
    , audioRecorder_(new QAudioRecorder(QCoreApplication::instance()))
    , audioProbe_(new QAudioProbe) {
  QObject::connect(audioProbe_,
                   SIGNAL(audioBufferProbed(QAudioBuffer)),
                   this,
                   SLOT(processBuffer(QAudioBuffer)));
  QObject::connect(audioRecorder_,
                   SIGNAL(durationChanged(qint64)),
                   this,
                   SLOT(updateProgress(qint64)));
  audioProbe_->setSource(audioRecorder_);
  device_ = audioRecorder_->audioInputs().first();
  LOG4CXX_INFO(logger_, "Add input device:" << device_.toStdString());

  audioRecorder_->setAudioInput(device_);
  codec_ = audioRecorder_->supportedAudioCodecs().first();

  LOG4CXX_INFO(logger_, "Set audio codec:" << codec_.toStdString());
  settings_.setCodec(codec_);
  settings_.setSampleRate(audioRecorder_->supportedAudioSampleRates().first());
  settings_.setBitRate(128000);
  settings_.setChannelCount(2);
  settings_.setEncodingMode(QMultimedia::ConstantBitRateEncoding);

  container_type_ = audioRecorder_->supportedContainers().first();
  audioRecorder_->setEncodingSettings(
      settings_, QVideoEncoderSettings(), container_type_);
  audioRecorder_->setOutputLocation(
      QUrl::fromLocalFile(QString(container.c_str())));
}

media_manager::FromMicToFileRecorderThread::Impl::~Impl() {
  delete audioRecorder_;
  audioRecorder_ = NULL;
  delete audioProbe_;
  audioProbe_ = NULL;
}

void media_manager::FromMicToFileRecorderThread::Impl::updateProgress(
    qint64 pos) {
  if (pos >= static_cast<qint64>(duration_)) {
    audioRecorder_->stop();
  }
}

const std::string
media_manager::FromMicToFileRecorderThread::Impl::getDeviceName() const {
  return device_.toStdString();
}

void media_manager::FromMicToFileRecorderThread::Impl::setDuration(
    const int32_t duration) {
  sync_primitives::AutoLock auto_lock(flagDuration_);
  duration_ = duration;
}

int32_t media_manager::FromMicToFileRecorderThread::Impl::getDuration() const {
  return duration_;
}

void media_manager::FromMicToFileRecorderThread::Impl::setShouldBeStoped(
    const bool shouldBeStoped) {
  sync_primitives::AutoLock auto_lock(stopFlagLock_);
  shouldBeStoped_ = shouldBeStoped;
}

void media_manager::FromMicToFileRecorderThread::Impl::startRecord() {
  if (audioRecorder_->isAvailable()) {
    audioRecorder_->record();
  }
}

void media_manager::FromMicToFileRecorderThread::Impl::stopRecord() {
  if (shouldBeStoped_) {
    audioRecorder_->stop();
  }
}

void media_manager::FromMicToFileRecorderThread::Impl::processBuffer(
    const QAudioBuffer& buffer) {
  QVector<qreal> levels = getBufferLevels(buffer);
}

void media_manager::FromMicToFileRecorderThread::Impl::displayErrorMessage() {
  LOG4CXX_ERROR(
      logger_,
      "QAudioRecorder eroor: " << audioRecorder_->errorString().toStdString());
}

////////////////////////////////////////////////////////////////////////////////
/// media_manager::FromMicToFileRecorderThread::Impl
////////////////////////////////////////////////////////////////////////////////

media_manager::FromMicToFileRecorderThread::FromMicToFileRecorderThread(
    const std::string& output_file, int32_t duration)
    : threads::ThreadDelegate()
    , impl_(new Impl(output_file))
    , sleepThread_(NULL)
    , outputFileName_(output_file) {
  LOG4CXX_AUTO_TRACE(logger_);
  set_record_duration(duration);
}

media_manager::FromMicToFileRecorderThread::~FromMicToFileRecorderThread() {
  LOG4CXX_AUTO_TRACE(logger_);
  delete impl_;
  impl_ = NULL;
  if (sleepThread_) {
    sleepThread_->join();
    delete sleepThread_->delegate();
    threads::DeleteThread(sleepThread_);
  }
}

void media_manager::FromMicToFileRecorderThread::set_record_duration(
    int32_t duration) {
  LOG4CXX_AUTO_TRACE(logger_);
  impl_->setDuration(duration);
}

void media_manager::FromMicToFileRecorderThread::threadMain() {
  LOG4CXX_AUTO_TRACE(logger_);

  impl_->setShouldBeStoped(false);

  if (outputFileName_.empty()) {
    LOG4CXX_ERROR(logger_, "Must supply destination");
  }

  LOG4CXX_TRACE(logger_, "Reading from device: " << impl_->getDeviceName());
  LOG4CXX_TRACE(logger_, "Saving pipeline output to: " << outputFileName_);
  LOG4CXX_TRACE(logger_, "Duration set to: " << impl_->getDuration());

  LOG4CXX_TRACE(logger_, "Audio capture started ...\n");

  if (impl_->getDuration() > 0) {
    sleepThread_ = threads::CreateThread(
        "SleepThread", new SleepThreadDelegate(impl_->getDuration()));
    sleepThread_->start();
    impl_->startRecord();
  }
}

media_manager::FromMicToFileRecorderThread::SleepThreadDelegate::
    SleepThreadDelegate(int32_t timeout)
    : threads::ThreadDelegate(), timeout_(timeout) {}

void media_manager::FromMicToFileRecorderThread::SleepThreadDelegate::
    threadMain() {
  LOG4CXX_TRACE(logger_, "Sleep for " << timeout_ << " seconds");
  threads::sleep(timeout_ * 1000);
}

void media_manager::FromMicToFileRecorderThread::exitThreadMain() {
  LOG4CXX_AUTO_TRACE(logger_);
  if (sleepThread_) {
    LOG4CXX_DEBUG(logger_, "Stop sleep thread\n");
    sleepThread_->stop();
  }

  LOG4CXX_TRACE(logger_, "Set should be stopped flag\n");
  impl_->setShouldBeStoped(true);
  impl_->stopRecord();
}

#include "from_mic_to_file_recorder_thread_qt.moc"
