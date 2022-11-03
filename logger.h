#pragma once

// project
#include "../task/pool.h"


// qt
#include <QtCore/QCoreApplication>
#include <QtCore/QDatetime>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QTextStream>


void Trace(const char *format, ...);
void TraceW(const wchar_t *format, ...);


#define WIN_DEBUGGER_TRACE(format, ...)                  \
			Trace(                                       \
				"[%s #%d] " ## format,                   \
				__FUNCTION__,                            \
                __LINE__,                                \
				__VA_ARGS__                              \
			)

#define WIN_DEBUGGER_TRACEW(format, ...)                 \
			TraceW(                                      \
				"[%s #%d] " ## format,                   \
				__FUNCTION__,                            \
                __LINE__,                                \
				__VA_ARGS__                              \
			)



#if defined(_DEBUG)
#define TRACE WIN_DEBUGGER_TRACE
#endif


// 将context字段设置为调用的日期时间
#define QtCurrentDateTimeStr  QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz").toUtf8().data()
#define LogDebugC             QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).debug
#define LogDebug              QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).debug().noquote
#define LogInfoC              QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).info
#define LogInfo               QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).info().noquote
#define LogWarningC           QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).warning
#define LogWarning            QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).warning().noquote
#define LogErrorC             QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).critical
#define LogError              QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).critical().noquote
#define LogFatalC             QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).fatal
#define LogFatal              QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, __FUNCTION__, QtCurrentDateTimeStr).fatal



class FileLogger : public QObject
{
	Q_OBJECT


private:
	FileLogger(qint64 nbytesAllowed, QtMsgType level, int flushIntervalMS);
	~FileLogger();

public:
	static FileLogger *GetInstance(qint64 nbytesAllowed = 10 * 1024 * 1024, QtMsgType level = QtInfoMsg, int flushIntervalMS = 1000);

	// 设置日志保存的目录和文件名
	bool SetPath(QString pathDir, QString fileName);
	// 设置日志文件最大大小
	void SetMaxBytesAllowed(qint64 nbytesAllowed);

	QString GetDirPath();

	void Install();

	// 外部调用，如果在多线程中调用，必须将lock设置为true
	bool Open(bool lock);
	// 外部调用，如果在多线程中调用，必须将lock设置为true
	bool Close(bool lock, bool reset=true);
	// 刷盘
	void Flush();
	// 外部调用，已加锁
	static void WriteMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg);



private:
	// 内部使用：不加锁
	bool Open();
	// 内部使用：不加锁
	bool Close();
	// 内部使用：不加锁，由上层调用加锁来保证
	bool RotateIfNeeded();


private:
	bool m_opened;
	bool m_reset;
	QFile m_file;
	QMutex m_mutex;
	QTextStream m_stream;

	QtMsgType m_level;
	int m_FlushIntervalMS;

	QString m_dir;
	QString m_path;
	qint64 m_nbytesAllowed;
};