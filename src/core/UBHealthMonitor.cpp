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

#include "UBHealthMonitor.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QTimer>
#include <QUndoStack>

#include <csignal>
#include <cstring>

#include "core/UB.h"
#include "core/UBApplication.h"
#include "core/UBSettings.h"

#include "board/UBBoardController.h"
#include "board/UBDrawingController.h"

#include "document/UBDocumentProxy.h"

#include "domain/UBGraphicsPolygonItem.h"
#include "domain/UBGraphicsScene.h"
#include "domain/UBGraphicsStrokesGroup.h"

#if defined(Q_OS_LINUX)
#include <cerrno>
#include <cstring>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

#include <chrono>

namespace {
// Tunables for the health report.
constexpr int SnapshotIntervalMs = 60 * 1000;
constexpr int HeartbeatIntervalMs = 1000;
constexpr int FreezeThresholdMs = 5000;
constexpr int SnapshotRetryMs = 3000;
constexpr int MaxBreadcrumbs = 200;
constexpr int MaxBreadcrumbLength = 191;
constexpr int MaxSnapshotLength = 4095;
constexpr int PolygonCountTimeBudgetMs = 3;
constexpr int MaxItemsForPolygonCount = 100000;

char g_sessionPath[4096] = {0};
char g_lastSnapshot[MaxSnapshotLength + 1] = {0};
std::mutex g_stateMutex;
std::mutex g_breadcrumbMutex;

struct Breadcrumb {
  char text[MaxBreadcrumbLength + 1];
};
Breadcrumb g_breadcrumbs[MaxBreadcrumbs];
std::atomic<unsigned> g_breadcrumbSeq{0};
std::atomic<bool> g_started{false};

#if defined(Q_OS_LINUX)
volatile sig_atomic_t g_inSignalHandler = 0;

void writeAll(int fd, const char *text) {
  if (!text)
    return;

  size_t length = 0;
  while (text[length] && length < 4095)
    ++length;

  size_t offset = 0;
  while (offset < length) {
    const ssize_t written = ::write(fd, text + offset, length - offset);
    if (written <= 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    offset += static_cast<size_t>(written);
  }
}

void writeInt(int fd, long value) {
  char buffer[32];
  int index = static_cast<int>(sizeof(buffer)) - 1;
  buffer[index] = '\0';

  if (value == 0) {
    buffer[--index] = '0';
  } else {
    const bool negative = value < 0;
    unsigned long magnitude = negative ? static_cast<unsigned long>(-value)
                                       : static_cast<unsigned long>(value);
    while (magnitude > 0 && index > 0) {
      buffer[--index] = static_cast<char>('0' + (magnitude % 10));
      magnitude /= 10;
    }
    if (negative && index > 0)
      buffer[--index] = '-';
  }

  writeAll(fd, buffer + index);
}

const char *signalName(int sig) {
  switch (sig) {
  case SIGSEGV:
    return "SIGSEGV";
  case SIGABRT:
    return "SIGABRT";
  case SIGBUS:
    return "SIGBUS";
  case SIGFPE:
    return "SIGFPE";
  case SIGILL:
    return "SIGILL";
  default:
    return "SIGUNKNOWN";
  }
}
#endif

QString timestamp() {
  return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
}

QString shortTimerTimestamp() {
  return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}

QString toolName(int tool) {
  switch (tool) {
  case UBStylusTool::Pen:
    return "Pen";
  case UBStylusTool::Eraser:
    return "Eraser";
  case UBStylusTool::Marker:
    return "Marker";
  case UBStylusTool::Pointer:
    return "Pointer";
  case UBStylusTool::Line:
    return "Line";
  case UBStylusTool::StrokeEraser:
    return "StrokeEraser";
  case UBStylusTool::Lasso:
    return "Lasso";
  case UBStylusTool::Laser:
    return "Laser";
  case UBStylusTool::Shape:
    return "Shape";
  case UBStylusTool::Selector:
    return "Selector";
  case UBStylusTool::Hand:
    return "Hand";
  default:
    return QString("Tool%1").arg(tool);
  }
}

struct ProcessStatus {
  long rssKb = -1;
  long vmKb = -1;
  int threads = -1;
};

ProcessStatus readProcessStatus() {
  ProcessStatus status;

#if defined(Q_OS_LINUX)
  QFile file("/proc/self/status");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return status;

  // /proc files report size 0, so QFile::atEnd() is immediately true. Read the
  // whole (small) file instead of using readLine()/atEnd().
  const QList<QByteArray> lines = file.readAll().split('\n');
  for (const QByteArray &line : lines) {
    if (line.startsWith("VmRSS:"))
      status.rssKb = line.mid(6).simplified().split(' ').value(0).toLong();
    else if (line.startsWith("VmSize:"))
      status.vmKb = line.mid(7).simplified().split(' ').value(0).toLong();
    else if (line.startsWith("Threads:"))
      status.threads = line.mid(8).simplified().toInt();
  }
#endif

  return status;
}

std::atomic<UBHealthMonitor *> s_instance{nullptr};
} // namespace

UBHealthMonitor &UBHealthMonitor::instance() {
  UBHealthMonitor *instance = s_instance.load();
  if (!instance) {
    instance = new UBHealthMonitor();
    s_instance.store(instance);
  }
  return *instance;
}

UBHealthMonitor::UBHealthMonitor(QObject *parent)
    : QObject(parent), m_fd(-1), m_snapshotTimer(nullptr),
      m_heartbeatTimer(nullptr), m_retryTimer(nullptr), m_deferredRetries(0),
      m_started(false), m_heartbeatMs(0), m_stopWatchdog(false),
      m_freezeReported(false) {}

UBHealthMonitor::~UBHealthMonitor() { stop(); }

bool UBHealthMonitor::isRunning() {
#if defined(Q_OS_LINUX)
  return g_started.load(std::memory_order_acquire);
#else
  return false;
#endif
}

void UBHealthMonitor::start() {
  if (m_started)
    return;

#if !defined(Q_OS_LINUX)
  // The health report currently uses Linux facilities (/proc, backtrace and
  // POSIX signals). Keep the calls harmless on other platforms.
  return;
#else
  const QString healthDirectory = UBSettings::userDataDirectory() + "/health";
  QDir().mkpath(healthDirectory);

  // Keep only the newest 10 session logs.
  QDir directory(healthDirectory);
  QStringList oldSessions = directory.entryList(
      QStringList() << "session-*.log", QDir::Files, QDir::Name);
  while (oldSessions.size() >= 10)
    directory.remove(oldSessions.takeFirst());

  m_sessionPath =
      healthDirectory + "/session-" +
      QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss-zzz") + ".log";

  const QByteArray path = m_sessionPath.toLocal8Bit();
  m_fd =
      ::open(path.constData(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);

  std::strncpy(g_sessionPath, path.constData(), sizeof(g_sessionPath) - 1);
  g_sessionPath[sizeof(g_sessionPath) - 1] = '\0';

  m_runningMarkerPath = healthDirectory + "/running";
  const bool previousSessionWasUnclean = QFile::exists(m_runningMarkerPath);
  {
    QFile marker(m_runningMarkerPath);
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      marker.write(QByteArray::number(QCoreApplication::applicationPid()));
      marker.close();
    }
  }

  // Preload the backtrace machinery so the signal handler does not do it
  // lazily. The link flags include -rdynamic so symbols are available.
  void *preload[1];
  backtrace(preload, 1);

  installSignalHandlers();

  m_started = true;
  g_started.store(true, std::memory_order_release);

  m_snapshotTimer = new QTimer(this);
  m_snapshotTimer->setTimerType(Qt::CoarseTimer);
  connect(m_snapshotTimer, &QTimer::timeout, this,
          [this] { writeSnapshot("periodic"); });
  m_snapshotTimer->start(SnapshotIntervalMs);

  m_heartbeatTimer = new QTimer(this);
  m_heartbeatTimer->setTimerType(Qt::CoarseTimer);
  connect(m_heartbeatTimer, &QTimer::timeout, this, [this] {
    m_heartbeatMs.store(QDateTime::currentMSecsSinceEpoch(),
                        std::memory_order_release);
  });
  m_heartbeatTimer->start(HeartbeatIntervalMs);

  m_retryTimer = new QTimer(this);
  m_retryTimer->setSingleShot(true);
  connect(m_retryTimer, &QTimer::timeout, this,
          [this] { writeSnapshot("retry"); });

  m_heartbeatMs.store(QDateTime::currentMSecsSinceEpoch(),
                      std::memory_order_release);

  writeLine(QString("[%1] SESSION START pid=%2 version=%3")
                .arg(timestamp())
                .arg(QCoreApplication::applicationPid())
                .arg(UBApplication::applicationVersion()));

  if (previousSessionWasUnclean)
    writeLine(
        QString("[%1] previous session did not exit cleanly").arg(timestamp()));

  // The board controller is created during UBApplication::exec(), before the
  // event loop starts, so connecting from the event loop is safe.
  QTimer::singleShot(0, this, [this] { connectSignals(); });

  startWatchdog();
#endif
}

void UBHealthMonitor::stop() {
  if (!m_started)
    return;

#if defined(Q_OS_LINUX)
  g_started.store(false, std::memory_order_release);
#endif

  m_started = false;

  if (m_snapshotTimer) {
    m_snapshotTimer->stop();
    m_snapshotTimer = nullptr;
  }
  if (m_heartbeatTimer) {
    m_heartbeatTimer->stop();
    m_heartbeatTimer = nullptr;
  }
  if (m_retryTimer) {
    m_retryTimer->stop();
    m_retryTimer = nullptr;
  }

  stopWatchdog();

  writeLine(QString("[%1] SESSION END pid=%2")
                .arg(timestamp())
                .arg(QCoreApplication::applicationPid()));

  {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    if (m_fd >= 0) {
#if defined(Q_OS_LINUX)
      ::close(m_fd);
#endif
      m_fd = -1;
    }
  }

  if (!m_runningMarkerPath.isEmpty())
    QFile::remove(m_runningMarkerPath);
}

void UBHealthMonitor::connectSignals() {
  UBBoardController *boardController = UBApplication::boardController;
  if (boardController) {
    connect(boardController, &UBBoardController::activeSceneChanged, this,
            [this] { snapshot("page-changed"); });
    connect(boardController, &UBBoardController::newPageAdded, this,
            [this] { snapshot("page-added"); });
  }

  UBDrawingController *drawingController =
      UBDrawingController::drawingController();
  if (drawingController) {
    connect(drawingController, &UBDrawingController::stylusToolChanged, this,
            [this](int tool, int previousTool) {
              breadcrumb(QString("tool changed %1 -> %2")
                             .arg(toolName(previousTool), toolName(tool)));
              snapshot("tool-changed");
            });
    connect(drawingController, &UBDrawingController::colorPaletteChanged, this,
            [this] { breadcrumb("palette changed"); });
  }

  if (UBApplication::undoStack) {
    connect(UBApplication::undoStack.data(), &QUndoStack::indexChanged, this,
            [this](int index) {
              breadcrumb(QString("undo/redo index=%1").arg(index));
            });
  }
}

void UBHealthMonitor::writeLine(const QString &line) {
  std::lock_guard<std::mutex> lock(m_writeMutex);
  if (m_fd < 0)
    return;

#if defined(Q_OS_LINUX)
  QByteArray bytes = line.toUtf8();
  bytes.append('\n');

  size_t offset = 0;
  while (offset < static_cast<size_t>(bytes.size())) {
    const ssize_t written = ::write(m_fd, bytes.constData() + offset,
                                    static_cast<size_t>(bytes.size()) - offset);
    if (written <= 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    offset += static_cast<size_t>(written);
  }
#else
  Q_UNUSED(line);
#endif
}

void UBHealthMonitor::writeSnapshot(const QString &reason, bool retryIfBusy) {
  if (!m_started)
    return;

  const ProcessStatus processStatus = readProcessStatus();

  UBBoardController *boardController = UBApplication::boardController;
  std::shared_ptr<UBGraphicsScene> scene =
      boardController ? boardController->activeScene() : nullptr;

  int itemCount = -1;
  int polygonCount = -1;
  bool countsDeferred = false;
  bool countsPartial = false;

  if (scene) {
    if (scene->inputDeviceIsPressed()) {
      countsDeferred = true;
    } else {
      itemCount = scene->itemCount();

      if (itemCount <= MaxItemsForPolygonCount) {
        QElapsedTimer timer;
        timer.start();

        int count = 0;
        const QList<QGraphicsItem *> allItems = scene->items();
        for (QGraphicsItem *item : allItems) {
          if (timer.elapsed() > PolygonCountTimeBudgetMs) {
            countsPartial = true;
            break;
          }

          if (item->type() == UBGraphicsStrokesGroup::Type)
            count += item->childItems().size();
          else if (item->type() == UBGraphicsPolygonItem::Type &&
                   !item->parentItem())
            ++count;
        }

        if (!countsPartial)
          polygonCount = count;
      } else {
        countsPartial = true;
      }
    }
  }

  const int undoCount =
      UBApplication::undoStack ? UBApplication::undoStack->count() : -1;
  const int pageCount = (boardController && boardController->selectedDocument())
                            ? boardController->selectedDocument()->pageCount()
                            : -1;
  const int tool = UBDrawingController::drawingController()
                       ? UBDrawingController::drawingController()->stylusTool()
                       : -1;
  const qreal zoom = boardController ? boardController->currentZoom() : -1.0;

  QString line =
      QString(
          "[%1] SNAPSHOT reason=%2 rss=%3kB vmsize=%4kB threads=%5 "
          "items=%6 strokePolygons=%7 undo=%8 pages=%9 tool=%10 zoom=%11%12")
          .arg(timestamp())
          .arg(reason)
          .arg(processStatus.rssKb)
          .arg(processStatus.vmKb)
          .arg(processStatus.threads)
          .arg(countsDeferred ? QString("-") : QString::number(itemCount))
          .arg(countsDeferred ? QString("-") : QString::number(polygonCount))
          .arg(undoCount)
          .arg(pageCount)
          .arg(tool >= 0 ? QString("%1(%2)").arg(toolName(tool)).arg(tool)
                         : QString("-"))
          .arg(zoom, 0, 'f', 2)
          .arg(countsDeferred ? " counts=deferred(pen-down)"
                              : (countsPartial ? " counts=partial" : ""));

  writeLine(line);

  {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const QByteArray bytes = line.toUtf8();
    const int length = qMin(bytes.size(), MaxSnapshotLength);
    std::memcpy(g_lastSnapshot, bytes.constData(), static_cast<size_t>(length));
    g_lastSnapshot[length] = '\0';
  }

  if (countsDeferred) {
    if (retryIfBusy && m_deferredRetries < 3 && m_retryTimer) {
      ++m_deferredRetries;
      m_retryTimer->start(SnapshotRetryMs);
    } else {
      m_deferredRetries = 0;
    }
  } else {
    m_deferredRetries = 0;
  }
}

void UBHealthMonitor::writeBreadcrumbs(const char *header) {
  std::lock_guard<std::mutex> lock(g_breadcrumbMutex);

  QStringList lines;
  lines << QString("--- %1 ---").arg(QString::fromLatin1(header));

  const unsigned sequence = g_breadcrumbSeq.load(std::memory_order_acquire);
  const unsigned first = sequence > static_cast<unsigned>(MaxBreadcrumbs)
                             ? sequence - MaxBreadcrumbs
                             : 0;

  for (unsigned i = first; i < sequence; ++i)
    lines << QString::fromUtf8(g_breadcrumbs[i % MaxBreadcrumbs].text);

  writeLine(lines.join('\n'));
}

void UBHealthMonitor::installSignalHandlers() {
#if defined(Q_OS_LINUX)
  struct sigaction action;
  std::memset(&action, 0, sizeof(action));
  action.sa_handler = &UBHealthMonitor::signalHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  sigaction(SIGSEGV, &action, nullptr);
  sigaction(SIGABRT, &action, nullptr);
  sigaction(SIGBUS, &action, nullptr);
  sigaction(SIGFPE, &action, nullptr);
  sigaction(SIGILL, &action, nullptr);
#endif
}

void UBHealthMonitor::startWatchdog() {
#if defined(Q_OS_LINUX)
  m_stopWatchdog.store(false, std::memory_order_release);
  m_freezeReported.store(false, std::memory_order_release);
  m_watchdog = std::thread([this] {
    qint64 lastHeartbeat = m_heartbeatMs.load(std::memory_order_acquire);

    while (!m_stopWatchdog.load(std::memory_order_acquire)) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (m_stopWatchdog.load(std::memory_order_acquire))
        break;

      const qint64 heartbeat = m_heartbeatMs.load(std::memory_order_acquire);
      const qint64 now = QDateTime::currentMSecsSinceEpoch();

      if (now - heartbeat > FreezeThresholdMs) {
        if (!m_freezeReported.exchange(true, std::memory_order_acq_rel)) {
          writeLine(QString("[%1] UI FROZEN (no heartbeat for %2 ms)")
                        .arg(timestamp())
                        .arg(now - heartbeat));

          QString lastSnapshot;
          {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            lastSnapshot = QString::fromUtf8(g_lastSnapshot);
          }
          if (!lastSnapshot.isEmpty())
            writeLine(QString("last snapshot: %1").arg(lastSnapshot));

          writeBreadcrumbs("UI FROZEN BREADCRUMBS");
        }
      } else if (heartbeat != lastHeartbeat) {
        lastHeartbeat = heartbeat;
        m_freezeReported.store(false, std::memory_order_release);
      }
    }
  });
#endif
}

void UBHealthMonitor::stopWatchdog() {
#if defined(Q_OS_LINUX)
  m_stopWatchdog.store(true, std::memory_order_release);
  if (m_watchdog.joinable())
    m_watchdog.join();
#endif
}

void UBHealthMonitor::breadcrumb(const QString &event) {
  if (!isRunning())
    return;

#if defined(Q_OS_LINUX)
  const QString text = QString("%1 %2").arg(shortTimerTimestamp(), event);
  QByteArray bytes = text.toUtf8();
  if (bytes.size() > MaxBreadcrumbLength)
    bytes.truncate(MaxBreadcrumbLength);

  std::lock_guard<std::mutex> lock(g_breadcrumbMutex);
  const unsigned sequence = g_breadcrumbSeq.load(std::memory_order_relaxed);
  Breadcrumb &entry = g_breadcrumbs[sequence % MaxBreadcrumbs];

  std::memcpy(entry.text, bytes.constData(), static_cast<size_t>(bytes.size()));
  entry.text[bytes.size()] = '\0';

  g_breadcrumbSeq.store(sequence + 1, std::memory_order_release);
#else
  Q_UNUSED(event);
#endif
}

void UBHealthMonitor::snapshot(const QString &reason) {
  UBHealthMonitor *instance = s_instance.load();
  if (instance && isRunning())
    instance->writeSnapshot(reason);
}

void UBHealthMonitor::logMessage(QtMsgType type, const QString &message) {
  if (type == QtDebugMsg)
    return;

  UBHealthMonitor *instance = s_instance.load();
  if (!instance || !instance->m_started)
    return;

  static thread_local bool inLogMessage = false;
  if (inLogMessage)
    return;

  inLogMessage = true;

  const char *level = "INFO";
  if (type == QtWarningMsg)
    level = "WARN";
  else if (type == QtCriticalMsg)
    level = "CRIT";
  else if (type == QtFatalMsg)
    level = "FATAL";

  QString text = message;
  if (text.size() > 4000)
    text.truncate(4000);

  instance->writeLine(QString("[%1] %2 %3").arg(timestamp(), level, text));

  inLogMessage = false;
}

void UBHealthMonitor::signalHandler(int sig) {
#if defined(Q_OS_LINUX)
  if (g_inSignalHandler)
    _exit(128 + sig);

  g_inSignalHandler = 1;

  int fd = -1;
  if (g_sessionPath[0] != '\0')
    fd = ::open(g_sessionPath, O_WRONLY | O_APPEND | O_CREAT, 0600);

  if (fd >= 0) {
    writeAll(fd, "\n*** CRASH ***\n");
    writeAll(fd, "signal=");
    writeInt(fd, sig);
    writeAll(fd, " ");
    writeAll(fd, signalName(sig));
    writeAll(fd, "\n");

    void *frames[64];
    const int frameCount = backtrace(frames, 64);
    writeAll(fd, "backtrace:\n");
    backtrace_symbols_fd(frames, frameCount, fd);

    writeAll(fd, "\nbreadcrumbs:\n");
    const unsigned sequence = g_breadcrumbSeq.load(std::memory_order_acquire);
    const unsigned first = sequence > static_cast<unsigned>(MaxBreadcrumbs)
                               ? sequence - MaxBreadcrumbs
                               : 0;
    for (unsigned i = first; i < sequence; ++i) {
      writeAll(fd, g_breadcrumbs[i % MaxBreadcrumbs].text);
      writeAll(fd, "\n");
    }

    writeAll(fd, "\nlast snapshot:\n");
    writeAll(fd, g_lastSnapshot);
    writeAll(fd, "\n");

    ::close(fd);
  }

  signal(sig, SIG_DFL);
  raise(sig);
#endif
}
