/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UBHEALTHMONITOR_H_
#define UBHEALTHMONITOR_H_

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <mutex>
#include <thread>

class QTimer;

// Lightweight session health report for OpenBoard.
//
// The normal path only appends a few lines to a session log (startup/exit,
// periodic snapshots, warnings) and keeps at most 200 breadcrumbs in memory.
// Breadcrumbs are written to the log only on a crash or a detected UI freeze.
class UBHealthMonitor : public QObject {
  Q_OBJECT

public:
  static UBHealthMonitor &instance();

  void start();
  void stop();

  static bool isRunning();
  static void breadcrumb(const QString &event);
  static void snapshot(const QString &reason);
  static void logMessage(QtMsgType type, const QString &message);

private:
  explicit UBHealthMonitor(QObject *parent = nullptr);
  ~UBHealthMonitor() override;

  void connectSignals();
  void writeLine(const QString &line);
  void writeSnapshot(const QString &reason, bool retryIfBusy = true);
  void writeBreadcrumbs(const char *header);
  void installSignalHandlers();
  void startWatchdog();
  void stopWatchdog();

  static void signalHandler(int sig);

  int m_fd;
  std::mutex m_writeMutex;
  QTimer *m_snapshotTimer;
  QTimer *m_heartbeatTimer;
  QTimer *m_retryTimer;
  int m_deferredRetries;
  bool m_started;
  QString m_sessionPath;
  QString m_runningMarkerPath;
  std::atomic<qint64> m_heartbeatMs;
  std::atomic<bool> m_stopWatchdog;
  std::atomic<bool> m_freezeReported;
  std::thread m_watchdog;
};

#endif /* UBHEALTHMONITOR_H_ */
