// self
#include "logger.h"

// qt
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>



// windows
#if defined(_WIN32)
#include <Windows.h>
#endif


void Trace(const char *format, ...)
{
	va_list args = NULL;
	va_start(args, format);
	size_t length = _vscprintf(format, args) + 1;
	char *buffer = new char[length];
	_vsnprintf_s(buffer, length, length, format, args);
	va_end(args);
	OutputDebugStringA(buffer);
	delete[] buffer;
}


void TraceW(const wchar_t *format, ...)
{
	va_list args = NULL;
	va_start(args, format);
	size_t length = _vscwprintf(format, args) + 1;
	wchar_t *buffer = new wchar_t[length];
	_vsnwprintf_s(buffer, length, length, format, args);
	va_end(args);
	OutputDebugStringW(buffer);
	delete[] buffer;
}



FileLogger::FileLogger(qint64 nbytesAllowed, QtMsgType level, int flushIntervalMS)
	: m_opened(false)
	, m_reset(true)
	, m_nbytesAllowed(nbytesAllowed)
	, m_level(level)
	, m_FlushIntervalMS(flushIntervalMS)
{
}


FileLogger::~FileLogger()
{
}


FileLogger *FileLogger::GetInstance(qint64 nbytesAllowed, QtMsgType level, int flushIntervalMS)
{
	static FileLogger instance(nbytesAllowed, level, flushIntervalMS);
	return &instance;
}


bool FileLogger::SetPath(QString pathDir, QString fileName)
{
	if (fileName.isEmpty()) {
		return false;
	}

	QRegularExpression regex_whitespace("\\s");

	fileName = fileName.replace(regex_whitespace, "_");

	if (pathDir.isEmpty()) {
		pathDir = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)[0].replace(regex_whitespace, "_");
		QDir().mkpath(pathDir);
	}

	m_dir = pathDir;
	m_path = pathDir.replace("\\", "/");
	if (!m_path.endsWith("/")) {
		m_path.append("/");
	}
	m_path.append(fileName);

	return true;
}


void FileLogger::SetMaxBytesAllowed(qint64 nbytesAllowed)
{
	m_nbytesAllowed = nbytesAllowed;
}


QString FileLogger::GetDirPath()
{
	return m_dir;
}


void FileLogger::Install()
{
	qInstallMessageHandler(this->WriteMessage);
}


// 不加锁
bool FileLogger::Open()
{
	m_file.setFileName(m_path);

#if defined(_DEBUG)
	bool status = m_file.open(QIODevice::WriteOnly | QIODevice::Truncate);
#else
	bool status = m_file.open(QIODevice::WriteOnly | QIODevice::Append);
#endif

	if (status) {
		m_opened = true;

		m_stream.setDevice(&m_file);

		TaskPool::GetInstance()->SubmitIntervalTask(
			"FlushFileLogger", this, 500,
			[](void *ptr) {
				FileLogger *pThis = (FileLogger *)ptr;
				pThis->Flush();
				return true;
			}
		);
	}

	return status;
}


bool FileLogger::Open(bool lock)
{
	if (!lock) {
		return Open();
	}
	else {
		QMutexLocker locker(&m_mutex);
		return Open();
	}
}


bool FileLogger::Close()
{
	if (!m_opened) {
		return false;
	}

	TaskPool::GetInstance()->RemoveIntervalTask("FlushFileLogger", false);

	if(m_reset) {
	    m_opened = false;
    }
	m_file.flush();
	m_file.close();
	return true;
}


bool FileLogger::Close(bool lock, bool reset)
{
	m_reset = reset;

	if (!lock) {
		return Close();
	}
	else {
		QMutexLocker locker(&m_mutex);
		return Close();
	}
}


void FileLogger::Flush()
{
	QMutexLocker locker(&m_mutex);

	if (!m_opened) {
		return;
	}

	m_stream.flush();

	RotateIfNeeded();
}


void FileLogger::WriteMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	FileLogger *pThis = GetInstance();

	QMutexLocker locker(&pThis->m_mutex);

	if (!pThis->m_opened) {
		if (!pThis->Open()) {
			return;
		}
	}

	QString level;
	switch (type) {
	case QtDebugMsg:
		level = "DEBUG";
		break;
	case QtWarningMsg:
		level = "WARNING";
		break;
	case QtCriticalMsg:
		level = "ERROR";
		break;
	case QtFatalMsg:
		level = "FATAL";
		break;
	case QtInfoMsg:
		level = "INFO";
		break;
	default:
		level = "UNKNOWN";
		break;
	}

	// [日期时间] [日志级别] [函数名 文件行号 进程号 线程号] 日志内容
	pThis->m_stream
		<< QString("[%1] [%2] [%3 #%4 %5 %6] ")
		.arg(isdigit(context.category[0]) ? context.category : QtCurrentDateTimeStr)  // 1
		.arg(level)                                                                   // 2
		.arg(context.function)                                                        // 3
		.arg(context.line)                                                            // 4
		.arg(QCoreApplication::applicationPid())                                      // 5
		.arg((qint64)QThread::currentThreadId())                                      // 6
		<< msg;

	// 遇到警告、错误、致命日志级别直接刷盘
	if (type != QtDebugMsg && type != QtInfoMsg) {
		pThis->m_stream.flush();
	}
}


bool FileLogger::RotateIfNeeded()
{
	if (!m_opened) {
		return false;
	}

	if (m_file.size() > m_nbytesAllowed) {
		Close();

		if (!m_file.rename(m_path + ".old")) {
			return false;
		}

		return Open();
	}

	return true;
}
