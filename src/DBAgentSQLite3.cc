/* Asura
   Copyright (C) 2013 MIRACLE LINUX CORPORATION
 
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

#include <cstring>
#include <cstdio>
#include <stdarg.h>

#include "Logger.h"
using namespace mlpl;

#include "DBAgentSQLite3.h"
#include "AsuraException.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static const char *TABLE_NAME_SYSTEM = "system";
static const char *TABLE_NAME_SERVERS = "servers";
static const char *TABLE_NAME_TRIGGERS = "triggers";

const int DBAgentSQLite3::DB_VERSION = 1;

typedef map<DBDomainId, string>     DBDomainIdPathMap;
typedef DBDomainIdPathMap::iterator DBDomainIdPathMapIterator;

struct DBAgentSQLite3::PrivateContext {
	static GMutex            mutex;
	static DBDomainIdPathMap domainIdPathMap;

	string        dbPath;
	sqlite3      *db;

	// methods
	PrivateContext(void)
	: db(NULL)
	{
	}

	static void lock(void)
	{
		g_mutex_lock(&mutex);
	}

	static void unlock(void)
	{
		g_mutex_unlock(&mutex);
	}
};

GMutex            DBAgentSQLite3::PrivateContext::mutex;
DBDomainIdPathMap DBAgentSQLite3::PrivateContext::domainIdPathMap;

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------
void DBAgentSQLite3::defineDBPath(DBDomainId domainId, const string &path)
{
	PrivateContext::lock();
	DBDomainIdPathMapIterator it =
	  PrivateContext::domainIdPathMap.find(domainId);
	if (it != PrivateContext::domainIdPathMap.end()) {
		it->second = path;
		PrivateContext::unlock();
		return;
	}

	pair<DBDomainIdPathMapIterator, bool> result =
	  PrivateContext::domainIdPathMap.insert
	    (pair<DBDomainId, string>(domainId, path));
	PrivateContext::unlock();

	ASURA_ASSERT(result.second,
	  "Failed to insert. Probably domain id (%u) is duplicated", domainId);
}

void DBAgentSQLite3::init(void)
{
	DBAgent::addSetupFunction(DefaultDBDomainId, defaultSetupFunc);
}

void DBAgentSQLite3::defaultSetupFunc(DBDomainId domainId)
{
	// We don't lock DB (use transaction) in the existence check of
	// a table and create it, because, this function is called in series.

	// search dbPath
	const string &dbPath = findDBPath(domainId);

	// TODO: check the DB version.
	//       If the DB version is old, update the content
	if (!isTableExisting(dbPath, TABLE_NAME_SYSTEM))
		createTableSystem(dbPath);
	else
		updateDBIfNeeded(dbPath);


	// check the servers table
	if (!isTableExisting(dbPath, TABLE_NAME_SERVERS))
		createTableServers(dbPath);

	// check the trigger table
	if (!isTableExisting(dbPath, TABLE_NAME_TRIGGERS))
		createTableTriggers(dbPath);
}

const string &DBAgentSQLite3::findDBPath(DBDomainId domainId)
{
	PrivateContext::lock();
	DBDomainIdPathMapIterator it =
	   PrivateContext::domainIdPathMap.find(domainId);
	PrivateContext::unlock();
	ASURA_ASSERT(it != PrivateContext::domainIdPathMap.end(),
	             "Not found DBPath: %u", domainId);
	return it->second;
}

bool DBAgentSQLite3::isTableExisting(const string &dbPath,
                                     const string &tableName)
{
	bool exist;
	sqlite3 *db = openDatabase(dbPath);
	try {
		exist = isTableExisting(db, tableName);
	} catch (...) {
		sqlite3_close(db);
		throw;
	}
	sqlite3_close(db);
	return exist;
}

DBAgentSQLite3::DBAgentSQLite3(DBDomainId domainId)
: DBAgent(domainId),
  m_ctx(NULL)
{
	// We don't lock DB (use transaction) in the existence check of
	m_ctx = new PrivateContext();
	m_ctx->dbPath = findDBPath(domainId);
	openDatabase();
}

DBAgentSQLite3::~DBAgentSQLite3()
{
	if (!m_ctx)
		return;

	if (m_ctx->db) {
		int result = sqlite3_close(m_ctx->db);
		if (result != SQLITE_OK) {
			// Should we throw an exception ?
			MLPL_ERR("Failed to close sqlite: %d\n", result);
		}
	}

	delete m_ctx;
}

bool DBAgentSQLite3::isTableExisting(const string &tableName)
{
	ASURA_ASSERT(m_ctx->db, "m_ctx->db is NULL");
	return isTableExisting(m_ctx->db, tableName);
}

bool DBAgentSQLite3::isRecordExisting(const string &tableName,
                                      const string &condition)
{
	int result;
	sqlite3_stmt *stmt;
	string query = StringUtils::sprintf(
	                 "SELECT * FROM %s WHERE %s",
	                 tableName.c_str(), condition.c_str());
	result = sqlite3_prepare(m_ctx->db, query.c_str(), query.size(),
	                         &stmt, NULL);
	if (result != SQLITE_OK) {
		sqlite3_finalize(stmt);
		THROW_ASURA_EXCEPTION(
		  "Failed to call sqlite3_prepare(): %d: %s",
		  result, query.c_str());
	}
	sqlite3_reset(stmt);
	bool found = false;
	while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
		found = true;
		break;
	}
	sqlite3_finalize(stmt);
	if (result != SQLITE_ROW && result != SQLITE_DONE) {
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_step(): %d",
		                      result);
	}
	return found;
}

void DBAgentSQLite3::addTargetServer
  (MonitoringServerInfo *monitoringServerInfo)
{
	string condition = StringUtils::sprintf("id=%u",
	                                        monitoringServerInfo->id);
	execSql("BEGIN");
	if (!isRecordExisting(TABLE_NAME_SERVERS, condition)) {
		execSql("INSERT INTO %s VALUES(%u,%d,%Q,%Q,%Q)",
		        TABLE_NAME_SERVERS,
		        monitoringServerInfo->id, monitoringServerInfo->type,
		        monitoringServerInfo->hostName.c_str(),
		        monitoringServerInfo->ipAddress.c_str(),
		        monitoringServerInfo->nickname.c_str());
	} else {
		execSql("UPDATE %s SET type=%d, hostname=%Q, "
		        "ip_address=%Q, nickname=%Q",
		        TABLE_NAME_SERVERS,
		        monitoringServerInfo->type,
		        monitoringServerInfo->hostName.c_str(),
		        monitoringServerInfo->ipAddress.c_str(),
		        monitoringServerInfo->nickname.c_str());
	}
	execSql("COMMIT");
}

void DBAgentSQLite3::getTargetServers
  (MonitoringServerInfoList &monitoringServers)
{
	execSql("BEGIN");

	int result;
	sqlite3_stmt *stmt;
	string query = "SELECT id, type, hostname, ip_address, nickname FROM ";
	query += TABLE_NAME_SERVERS;
	result = sqlite3_prepare(m_ctx->db, query.c_str(), query.size(),
	                         &stmt, NULL);
	if (result != SQLITE_OK) {
		sqlite3_finalize(stmt);
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_prepare(): %d",
		                      result);
	}
	sqlite3_reset(stmt);
	while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
		monitoringServers.push_back(MonitoringServerInfo());
		MonitoringServerInfo &svInfo = monitoringServers.back();
		svInfo.id   = sqlite3_column_int(stmt, 0);
		svInfo.type = (MonitoringSystemType)sqlite3_column_int(stmt, 1);
		svInfo.hostName  = (const char *)sqlite3_column_text(stmt, 2);
		svInfo.ipAddress = (const char *)sqlite3_column_text(stmt, 3);
		svInfo.nickname  = (const char *)sqlite3_column_text(stmt, 4);
	}
	sqlite3_finalize(stmt);
	if (result != SQLITE_DONE) {
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_step(): %d",
		                      result);
	}

	execSql("COMMIT");
}

void DBAgentSQLite3::addTriggerInfo(TriggerInfo *triggerInfo)
{
	string condition = StringUtils::sprintf("id=%"PRIu64, triggerInfo->id);
	execSql("BEGIN");
	if (!isRecordExisting(TABLE_NAME_TRIGGERS, condition)) {
		execSql("INSERT INTO %s VALUES(%lu,%d,%d,%d,%d,%u,%Q,%Q,%Q)",
		        TABLE_NAME_TRIGGERS, triggerInfo->id,
		        triggerInfo->status, triggerInfo->severity,
		        triggerInfo->lastChangeTime.tv_sec, 
		        triggerInfo->lastChangeTime.tv_nsec, 
		        triggerInfo->serverId,
		        triggerInfo->hostId.c_str(),
		        triggerInfo->hostName.c_str(),
		        triggerInfo->brief.c_str());
	} else {
		execSql("UPDATE %s SET status=%d, severity=%d, "
		        "last_change_time_sec=%d, last_change_time_ns=%d, "
		        "server_id=%u, host_id=%Q, host_name=%Q, brief=%Q",
		        TABLE_NAME_TRIGGERS,
		        triggerInfo->status, triggerInfo->severity,
		        triggerInfo->lastChangeTime.tv_sec, 
		        triggerInfo->lastChangeTime.tv_nsec, 
		        triggerInfo->serverId,
		        triggerInfo->hostId.c_str(),
		        triggerInfo->hostName.c_str(),
		        triggerInfo->brief.c_str());
	}
	execSql("COMMIT");
}

void DBAgentSQLite3::getTriggerInfoList(TriggerInfoList &triggerInfoList)
{
	execSql("BEGIN");

	int result;
	sqlite3_stmt *stmt;
	string query = "SELECT id, status, severity, last_change_time_sec, "
	               "last_change_time_ns, server_id, host_id, host_name, "
	               "brief FROM ";
	query += TABLE_NAME_TRIGGERS;
	result = sqlite3_prepare(m_ctx->db, query.c_str(), query.size(),
	                         &stmt, NULL);
	if (result != SQLITE_OK) {
		sqlite3_finalize(stmt);
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_prepare(): %d",
		                      result);
	}
	sqlite3_reset(stmt);
	while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
		triggerInfoList.push_back(TriggerInfo());
		TriggerInfo &trigInfo = triggerInfoList.back();
		trigInfo.id   = sqlite3_column_int(stmt, 0);
		trigInfo.status =
		   (TriggerStatusType)sqlite3_column_int(stmt, 1);
		trigInfo.severity =
		   (TriggerSeverityType)sqlite3_column_int(stmt, 2);
		trigInfo.lastChangeTime.tv_sec  = sqlite3_column_int(stmt, 3);
		trigInfo.lastChangeTime.tv_nsec = sqlite3_column_int(stmt, 4);
		trigInfo.serverId = sqlite3_column_int(stmt, 5);
		trigInfo.hostId   = (const char *)sqlite3_column_text(stmt, 6);
		trigInfo.hostName = (const char *)sqlite3_column_text(stmt, 7);
		trigInfo.brief    = (const char *)sqlite3_column_text(stmt, 8);
	}
	sqlite3_finalize(stmt);
	if (result != SQLITE_DONE) {
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_step(): %d",
		                      result);
	}

	execSql("COMMIT");
}

int DBAgentSQLite3::getDBVersion(void)
{
	ASURA_ASSERT(m_ctx->db, "m_ctx->db is NULL");
	return getDBVersion(m_ctx->db);
}

void DBAgentSQLite3::createTable(TableCreationArg &tableCreationArg)
{
	vector<size_t> multipleKeyColumnIndexVector;
	vector<size_t> uniqueKeyColumnIndexVector;

	// make a SQL statement
	string sql = "CREATE TABLE ";
	sql += tableCreationArg.tableName;
	sql += "(";
	for (size_t i = 0; i < tableCreationArg.numColumns; i++) {
		ColumnDef &columnDef = tableCreationArg.columnDefs[i];

		// set type
		sql += columnDef.columnName;
		sql += " ";
		switch (columnDef.type) {
		case SQL_COLUMN_TYPE_INT:
		case SQL_COLUMN_TYPE_BIGUINT:
			sql += "INTEGER";
			break;
		case SQL_COLUMN_TYPE_VARCHAR:
		case SQL_COLUMN_TYPE_CHAR:
		case SQL_COLUMN_TYPE_TEXT:
			sql += "TEXT";
			break;
		case SQL_COLUMN_TYPE_DOUBLE:
			sql += "REAL";
			break;
		default:
			ASURA_ASSERT(true, "Unknown column type: %d (%s)",
			             columnDef.type, columnDef.columnName);
		}
		sql += " ";

		// set key
		switch (columnDef.keyType) {
		case SQL_KEY_PRI:
			sql += "PRIMARY KEY";
			break;
		case SQL_KEY_MUL:
			multipleKeyColumnIndexVector.push_back(i);
			break;
		case SQL_KEY_UNI:
			uniqueKeyColumnIndexVector.push_back(i);
			break;
		case SQL_KEY_NONE:
			break;
		default:
			ASURA_ASSERT(true, "Unknown column type: %d (%s)",
			             columnDef.keyType, columnDef.columnName);
		}

		if (i < tableCreationArg.numColumns - 1)
			sql += ",";
	}
	sql += ")";

	// exectute the SQL statement
	char *errmsg;
	int result = sqlite3_exec(m_ctx->db, sql.c_str(), NULL, NULL, &errmsg);
	if (result != SQLITE_OK) {
		string err = errmsg;
		sqlite3_free(errmsg);
		THROW_ASURA_EXCEPTION("Failed to exec: %d, %s, %s",
		                      result, err.c_str(), sql.c_str());
	}

	// add indexes
	if (!multipleKeyColumnIndexVector.empty()) {
		bool isUniqueKey = false;
		createIndex(tableCreationArg.tableName,
		            tableCreationArg.columnDefs, "multiple_index",
		            multipleKeyColumnIndexVector, isUniqueKey);
	}

	if (!uniqueKeyColumnIndexVector.empty()) {
		bool isUniqueKey = true;
		createIndex(tableCreationArg.tableName,
		            tableCreationArg.columnDefs, "unique_index",
		            uniqueKeyColumnIndexVector, isUniqueKey);
	}
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------
sqlite3 *DBAgentSQLite3::openDatabase(const string &dbPath)
{
	sqlite3 *db = NULL;
	int result = sqlite3_open_v2(dbPath.c_str(), &db,
	                             SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,
	                             NULL);
	if (result != SQLITE_OK) {
		THROW_ASURA_EXCEPTION("Failed to open sqlite: %d, %s",
		                      result, dbPath.c_str());
	}
	return db;
}

int DBAgentSQLite3::getDBVersion(const string &dbPath)
{
	int version;
	sqlite3 *db = openDatabase(dbPath);
	try {
		version = getDBVersion(db);
	} catch (...) {
		sqlite3_close(db);
		throw;
	}
	sqlite3_close(db);
	return version;
}

int DBAgentSQLite3::getDBVersion(sqlite3 *db)
{
	int result;
	sqlite3_stmt *stmt;
	string query = "SELECT version FROM ";
	query += TABLE_NAME_SYSTEM;
	result = sqlite3_prepare(db, query.c_str(), query.size(),
	                         &stmt, NULL);
	if (result != SQLITE_OK) {
		sqlite3_finalize(stmt);
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_prepare(): %d",
		                      result);
	}
	sqlite3_reset(stmt);
	int version = 0;
	int count = 0;
	while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
		version = sqlite3_column_int(stmt, 0);
		count++;
	}
	if (result != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_step(): %d",
		                      result);
	}
	sqlite3_finalize(stmt);
	ASURA_ASSERT(count == 1,
	             "Returned count of rows is not one (%d)", count);
	return version;
}

bool DBAgentSQLite3::isTableExisting(sqlite3 *db,
                                     const string &tableName)
{
	int result;
	sqlite3_stmt *stmt;
	const char *query = "SELECT COUNT(*) FROM sqlite_master "
	                    "WHERE type='table' AND name=?";
	result = sqlite3_prepare(db, query, strlen(query), &stmt, NULL);
	if (result != SQLITE_OK) {
		sqlite3_finalize(stmt);
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_prepare(): %d",
		                      result);
	}

	sqlite3_reset(stmt);
	result = sqlite3_bind_text(stmt, 1, tableName.c_str(),
	                           -1, SQLITE_STATIC);
	if (result != SQLITE_OK) {
		sqlite3_finalize(stmt);
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_bind(): %d",
		                      result);
	}
	
	int count = 0;
	while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	}
	if (result != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		THROW_ASURA_EXCEPTION("Failed to call sqlite3_step(): %d",
		                      result);
	}
	sqlite3_finalize(stmt);
	return count > 0;
}

void DBAgentSQLite3::updateDBIfNeeded(const string &dbPath)
{
	if (getDBVersion(dbPath) == DB_VERSION)
		return;
	THROW_ASURA_EXCEPTION("Not implemented: %s", __PRETTY_FUNCTION__);
}

void DBAgentSQLite3::createTableSystem(const string &dbPath)
{
	sqlite3 *db = openDatabase(dbPath);

	// make table
	string sql = "CREATE TABLE ";
	sql += TABLE_NAME_SYSTEM;
	sql += "(version INTEGER)";
	char *errmsg;
	int result = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errmsg);
	if (result != SQLITE_OK) {
		string err = errmsg;
		sqlite3_free(errmsg);
		sqlite3_close(db);
		THROW_ASURA_EXCEPTION("Failed to exec: %d, %s, %s",
		                      result, err.c_str(), sql.c_str());
	}

	// insert the version
	sql = StringUtils::sprintf("INSERT INTO %s VALUES(%d)",
	                           TABLE_NAME_SYSTEM,  DB_VERSION);
	result = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errmsg);
	if (result != SQLITE_OK) {
		string err = errmsg;
		sqlite3_free(errmsg);
		sqlite3_close(db);
		THROW_ASURA_EXCEPTION("Failed to exec: %d, %s, %s",
		                      result, err.c_str(), sql.c_str());
	}

	sqlite3_close(db);
}

void DBAgentSQLite3::createTableServers(const string &dbPath)
{
	sqlite3 *db = openDatabase(dbPath);

	// make table
	string sql = "CREATE TABLE ";
	sql += TABLE_NAME_SERVERS;
	sql += "(id INTEGER PRIMARY KEY, type INTEGER, hostname TEXT, "
	       " ip_address TEXT, nickname TEXT)";
	char *errmsg;
	int result = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errmsg);
	if (result != SQLITE_OK) {
		string err = errmsg;
		sqlite3_free(errmsg);
		sqlite3_close(db);
		THROW_ASURA_EXCEPTION("Failed to exec: %d, %s, %s",
		                      result, err.c_str(), sql.c_str());
	}

	sqlite3_close(db);
}

void DBAgentSQLite3::createTableTriggers(const string &dbPath)
{
	sqlite3 *db = openDatabase(dbPath);

	string sql = "CREATE TABLE ";
	sql += TABLE_NAME_TRIGGERS;
	sql += "(id INTEGER PRIMARY KEY, status INTEGER, severity INTEGER, "
	       " last_change_time_sec INTEGER, last_change_time_ns INTERGET, "
	       " server_id INTEGER, host_id TEXT, host_name TEXT, brief TEXT)";
	char *errmsg;
	int result = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errmsg);
	if (result != SQLITE_OK) {
		string err = errmsg;
		sqlite3_free(errmsg);
		sqlite3_close(db);
		THROW_ASURA_EXCEPTION("Failed to exec: %d, %s, %s",
		                      result, err.c_str(), sql.c_str());
	}

	sqlite3_close(db);
}


//
// Non static methods
//
void DBAgentSQLite3::openDatabase(void)
{
	if (m_ctx->db)
		return;

	ASURA_ASSERT(!m_ctx->dbPath.empty(), "dbPath is empty.");
	m_ctx->db = openDatabase(m_ctx->dbPath);
}

void DBAgentSQLite3::execSql(const char *fmt, ...)
{
	// make a query string
	va_list ap;
	va_start(ap, fmt);
	char *sql = sqlite3_vmprintf(fmt, ap);
	va_end(ap);

	// execute the query
	char *errmsg;
	int result = sqlite3_exec(m_ctx->db, sql, NULL, NULL, &errmsg);
	if (result != SQLITE_OK) {
		string err = errmsg;
		string sqlStr = sql;
		sqlite3_free(errmsg);
		sqlite3_free(sql);
		THROW_ASURA_EXCEPTION("Failed to exec: %d, %s, %s",
		                      result, err.c_str(), sqlStr.c_str());
	}
	sqlite3_free(sql);
}

void DBAgentSQLite3::createIndex(const string &tableName, 
                                 ColumnDef *columnDefs, const string &indexName,
                                 const vector<size_t> &targetIndexes,
                                 bool isUniqueKey)
{
	ASURA_ASSERT(!targetIndexes.empty(), "target indexes vector is empty.");

	// make an SQL statement
	string sql = "CREATE ";
	if (isUniqueKey)
		sql += "UNIQUE ";
	sql += "INDEX ";
	sql += indexName;
	sql += " ON ";
	sql += tableName;
	sql += "(";
	for (size_t i = 0; i < targetIndexes.size(); i++) {
		ColumnDef &columnDef = columnDefs[i];
		sql += columnDef.columnName;
		if (i < targetIndexes.size() - 1)
			sql += ",";
	}
	sql += ")";

	// execute the SQL statement
	char *errmsg;
	int result = sqlite3_exec(m_ctx->db, sql.c_str(), NULL, NULL, &errmsg);
	if (result != SQLITE_OK) {
		string err = errmsg;
		sqlite3_free(errmsg);
		THROW_ASURA_EXCEPTION("Failed to exec: %d, %s, %s",
		                      result, err.c_str(), sql.c_str());
	}
}

