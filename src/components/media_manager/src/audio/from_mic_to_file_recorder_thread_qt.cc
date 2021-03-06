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

  QAtomicInt duration_;
  bool should_be_stoped_;
  sync_primitives::Lock stopFlagLock_;

 private:
  QAudioEncoderSettings settings_;
  QAudioRecorder* audioRecorder_;
  DISALLOW_COPY_AND_ASSIGN(Impl);
};
}  // namespace media_manager

media_manager::FromMicToFileRecorderThread::Impl::Impl(
    const std::string& container)
    : settings_()
    , audioRecorder_(new QAudioRecorder(QCoreApplication::instance())) {
  QObject::connect(audioRecorder_,
                   SIGNAL(durationChanged(qint64)),
                   this,
                   SLOT(updateProgress(qint64)));
  LOGGER_INFO(logger_, "Add input device:" << device_.toStdString());

  device_ = audioRecorder_->audioInputs().first();
  audioRecorder_->setAudioInput(device_);

  codec_ = audioRecorder_->supportedAudioCodecs().first();
  LOGGER_INFO(logger_, "Set audio codec:" << codec_.toStdString());
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
  duration_ = duration;
}

int32_t media_manager::FromMicToFileRecorderThread::Impl::getDuration() const {
  return duration_;
}

void media_manager::FromMicToFileRecorderThread::Impl::setShouldBeStoped(
    const bool shouldBeStoped) {
  sync_primitives::AutoLock auto_lock(stopFlagLock_);
  should_be_stoped_ = shouldBeStoped;
}

void media_manager::FromMicToFileRecorderThread::Impl::startRecord() {
  if (audioRecorder_->isAvailable()) {
    audioRecorder_->record();
  }
}

void media_manager::FromMicToFileRecorderThread::Impl::stopRecord() {
  if (should_be_stoped_) {
    audioRecorder_->stop();
  }
}

void media_manager::FromMicToFileRecorderThread::Impl::displayErrorMessage() {
  LOGGER_ERROR(
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
    , output_file_name_(output_file) {
  LOGGER_AUTO_TRACE(logger_);
  setRecordDuration(duration);
  sleep_thread_ = FromMicToFileRecorderThreadPtr(
      new timer::TimerThread<FromMicToFileRecorderThread>(
          "AudioFromMicSuspend",
          this,
          &FromMicToFileRecorderThread::onFromMicToFileRecorderThreadSuspned,
          true));
}

media_manager::FromMicToFileRecorderThread::~FromMicToFileRecorderThread() {
  LOGGER_AUTO_TRACE(logger_);
  delete impl_;
  impl_ = NULL;
  if (sleep_thread_) {
    sleep_thread_->suspend();
  }
}

void media_manager::FromMicToFileRecorderThread::setRecordDuration(
    int32_t duration) {
  LOGGER_AUTO_TRACE(logger_);
  impl_->setDuration(duration);
}

void media_manager::FromMicToFileRecorderThread::threadMain() {
  LOGGER_AUTO_TRACE(logger_);

  impl_->setShouldBeStoped(false);

  if (output_file_name_.empty()) {
    LOGGER_ERROR(logger_, "Must supply destination");
  }

  LOGGER_TRACE(logger_, "Reading from device: " << impl_->getDeviceName());
  LOGGER_TRACE(logger_, "Saving pipeline output to: " << output_file_name_);
  LOGGER_TRACE(logger_, "Duration set to: " << impl_->getDuration());

  LOGGER_TRACE(logger_, "Audio capture started ...\n");

  if (impl_->getDuration() > 0) {
    sleep_thread_->start(impl_->getDuration());
    impl_->startRecord();
  }
}

void media_manager::FromMicToFileRecorderThread::
    onFromMicToFileRecorderThreadSuspned() {
  LOGGER_AUTO_TRACE(logger_);
  impl_->stopRecord();
  LOGGER_TRACE(logger_, "Set should be stopped flag\n");
  impl_->setShouldBeStoped(true);
}

void media_manager::FromMicToFileRecorderThread::exitThreadMain() {
  LOGGER_AUTO_TRACE(logger_);
  if (sleep_thread_) {
    LOGGER_DEBUG(logger_, "Stop sleep thread\n");
    sleep_thread_->stop();
  }

  LOGGER_TRACE(logger_, "Set should be stopped flag\n");
  impl_->setShouldBeStoped(true);
  impl_->stopRecord();
}

#include "from_mic_to_file_recorder_thread_qt.moc"
