#include "util/db/dbconnectionpool.h"

#include "util/memory.h"
#include "util/logger.h"


namespace mixxx {

namespace {

const mixxx::Logger kLogger("DbConnectionPool");

} // anonymous namespace

bool DbConnectionPool::createThreadLocalConnection() {
    VERIFY_OR_DEBUG_ASSERT(!m_threadLocalConnections.hasLocalData()) {
        DEBUG_ASSERT(m_threadLocalConnections.localData());
        kLogger.critical()
                << "Thread-local database connection already exists"
                << *m_threadLocalConnections.localData();
        return false; // abort
    }
    const int connectionIndex =
            m_connectionCounter.fetchAndAddAcquire(1) + 1;
    const QString indexedConnectionName =
            QString("%1-%2").arg(
                    m_prototypeConnection.name(),
                    QString::number(connectionIndex));
    auto pConnection = std::make_unique<DbConnection>(m_prototypeConnection, indexedConnectionName);
    if (!pConnection->open()) {
        kLogger.critical()
                << "Failed to open thread-local database connection"
                << *pConnection;
        return false; // abort
    }
    m_threadLocalConnections.setLocalData(pConnection.get()); // transfer ownership
    pConnection.release(); // release ownership
    DEBUG_ASSERT(m_threadLocalConnections.hasLocalData());
    DEBUG_ASSERT(m_threadLocalConnections.localData());
    kLogger.info()
            << "Cloned thread-local database connection"
            << *m_threadLocalConnections.localData();
    return true;
}

void DbConnectionPool::destroyThreadLocalConnection() {
    m_threadLocalConnections.setLocalData(nullptr);
}

QSqlDatabase DbConnectionPool::threadLocalConnection() const {
    VERIFY_OR_DEBUG_ASSERT(m_threadLocalConnections.hasLocalData()) {
        kLogger.critical()
                << "Thread-local database connection not available";
    }
    DbConnection* pConnection = m_threadLocalConnections.localData();
    VERIFY_OR_DEBUG_ASSERT(pConnection) {
        kLogger.critical()
                << "Thread-local database connection not valid";
        return QSqlDatabase(); // abort
    }
    kLogger.debug()
            << "Found thread-local database connection"
            << *pConnection;;
    return *pConnection;
}

DbConnectionPool::DbConnectionPool(
        const DbConnection::Params& params,
        const QString& connectionName)
    : m_prototypeConnection(params, connectionName),
      m_connectionCounter(0) {
}

DbConnectionPool::ThreadLocalScope::ThreadLocalScope(
        DbConnectionPoolPtr pDbConnectionPool) {
    if (pDbConnectionPool && pDbConnectionPool->createThreadLocalConnection()) {
        m_pDbConnectionPool = std::move(pDbConnectionPool);
        m_sqlDatabase = m_pDbConnectionPool->threadLocalConnection();
    }
}

DbConnectionPool::ThreadLocalScope::~ThreadLocalScope() {
    if (m_pDbConnectionPool) {
        m_pDbConnectionPool->destroyThreadLocalConnection();
    }
}

} // namespace mixxx
