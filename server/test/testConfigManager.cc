/*
 * Copyright (C) 2013-2014 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gcutter.h>
#include <cppcutter.h>
#include <StringUtils.h>
#include <errno.h>
#include "config.h"
#include "ConfigManager.h"
#include "Hatohol.h"
#include "Helpers.h"
using namespace std;
using namespace mlpl;

namespace testConfigManager {

void cut_setup(void)
{
	hatoholInit();
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------
void test_getActionCommandDirectory(void)
{
	string expect = LIBEXECDIR "/" PACKAGE "/action";
	cppcut_assert_equal(
	  expect, ConfigManager::getInstance()->getActionCommandDirectory());
}

void test_setActionCommandDirectory(void)
{
	string exampleDir = "/usr/hoge/example/dog";
	ConfigManager::getInstance()->setActionCommandDirectory(exampleDir);
	cppcut_assert_equal(
	  exampleDir,
	  ConfigManager::getInstance()->getActionCommandDirectory());
}

void test_getResidentYardDirectory(void)
{
	string expect = PREFIX "/sbin";
	cppcut_assert_equal(
	  expect, ConfigManager::getInstance()->getResidentYardDirectory());
}

void test_setResidentYardDirectory(void)
{
	string exampleDir = "/usr/hoge/example/dog";
	ConfigManager::getInstance()->setResidentYardDirectory(exampleDir);
	cppcut_assert_equal(
	  exampleDir, ConfigManager::getInstance()->getResidentYardDirectory());
}

void test_parseConfigServerDefault(void)
{
	ConfigManager *confMgr = ConfigManager::getInstance();
	cppcut_assert_equal(string("localhost"), confMgr->getDBServerAddress());
	cppcut_assert_equal(0, confMgr->getDBServerPort());
}

void test_parseConfigServer(void)
{
	CommandArgHelper cmds;
	cmds << "--db-server";
	cmds << "umi.example.com";
	cmds.activate();
	ConfigManager *confMgr = ConfigManager::getInstance();
	cppcut_assert_equal(string("umi.example.com"),
	                    confMgr->getDBServerAddress());
	cppcut_assert_equal(0, confMgr->getDBServerPort());
}

void test_parseConfigServerWithPort(void)
{
	CommandArgHelper cmds;
	cmds << "--db-server";
	cmds << "umi.example.com:3333";
	cmds.activate();
	ConfigManager *confMgr = ConfigManager::getInstance();
	cppcut_assert_equal(string("umi.example.com"),
	                    confMgr->getDBServerAddress());
	cppcut_assert_equal(3333, confMgr->getDBServerPort());
}

void test_parseTestModeDefault(void)
{
	cppcut_assert_equal(false, ConfigManager::getInstance()->isTestMode());
}

void test_parseTestModeEnabled(void)
{
	CommandArgHelper cmds;
	cmds << "--test-mode";
	cmds.activate();
	cppcut_assert_equal(true, ConfigManager::getInstance()->isTestMode());
}

void test_parseCopyOnDemandDefault(void)
{
	cppcut_assert_equal(
	  ConfigManager::UNKNOWN,
	  ConfigManager::getInstance()->getCopyOnDemand());
}

void test_parseEnableCopyOnDemand(void)
{
	CommandArgHelper cmds;
	cmds << "--enable-copy-on-demand";
	cmds.activate();
	cppcut_assert_equal(
	  ConfigManager::ENABLE,
	  ConfigManager::getInstance()->getCopyOnDemand());
}

void test_parseDisableCopyOnDemand(void)
{
	CommandArgHelper cmds;
	cmds << "--disable-copy-on-demand";
	cmds.activate();
	cppcut_assert_equal(
	  ConfigManager::DISABLE,
	  ConfigManager::getInstance()->getCopyOnDemand());
}

void test_parseFaceRestPortDefault(void)
{
	cppcut_assert_equal(
	 0, ConfigManager::getInstance()->getFaceRestPort());
}

void test_parseFaceRestPort(void)
{
	CommandArgHelper cmds;
	cmds << "--face-rest-port";
	cmds << "12345";
	cmds.activate();
	cppcut_assert_equal(12345,
	                    ConfigManager::getInstance()->getFaceRestPort());
}

void data_parseFaceRestPortWithInvalValue(void)
{
	gcut_add_datum("Big",      "port", G_TYPE_INT, 65536, NULL);
	gcut_add_datum("Negative", "port", G_TYPE_INT, -1, NULL);
	gcut_add_datum("Zero",     "port", G_TYPE_INT, 0, NULL);
}

void test_parseFaceRestPortWithInvalValue(gconstpointer data)
{
	string portStr =
	   StringUtils::sprintf("%d", gcut_data_get_int(data, "port"));
	CommandArgHelper cmds;
	cmds << "--face-rest-port";
	cmds << portStr.c_str();
	cmds.activate();
	cppcut_assert_equal(
	 0, ConfigManager::getInstance()->getFaceRestPort());
}

void test_parsePidFilePathDefault(void)
{
	cppcut_assert_equal(
	  string(ConfigManager::DEFAULT_PID_FILE_PATH),
	  ConfigManager::getInstance()->getPidFilePath());
}

void test_parsePidFilePath(void)
{
	CommandArgHelper cmds;
	cmds << "--pid-file";
	cmds << "/tmp/hoge/foo.x";
	cmds.activate();
	cppcut_assert_equal(
	  string("/tmp/hoge/foo.x"),
	  ConfigManager::getInstance()->getPidFilePath());
}

void test_setFaceRestPort(void)
{
	const int port = 2323;
	ConfigManager::getInstance()->setFaceRestPort(port);
	cppcut_assert_equal(port,
	                    ConfigManager::getInstance()->getFaceRestPort());
}

void data_parseLogLevel(void)
{
	gcut_add_datum("DBG",
	               "level", G_TYPE_STRING, "DBG",
	               "expectSuccess", G_TYPE_BOOLEAN, TRUE, NULL);
	gcut_add_datum("INFO",
	               "level", G_TYPE_STRING, "INFO",
	               "expectSuccess", G_TYPE_BOOLEAN, TRUE, NULL);
	gcut_add_datum("WARN",
	               "level", G_TYPE_STRING, "WARN",
	               "expectSuccess", G_TYPE_BOOLEAN, TRUE, NULL);
	gcut_add_datum("ERR",
	               "level", G_TYPE_STRING, "ERR",
	               "expectSuccess", G_TYPE_BOOLEAN, TRUE, NULL);
	gcut_add_datum("CRIT",
	               "level", G_TYPE_STRING, "CRIT",
	               "expectSuccess", G_TYPE_BOOLEAN, TRUE, NULL);
	gcut_add_datum("BUG",
	               "level", G_TYPE_STRING, "BUG",
	               "expectSuccess", G_TYPE_BOOLEAN, TRUE, NULL);
	gcut_add_datum("INVALID_STRING",
	               "level", G_TYPE_STRING, "INVALID_STRING",
	               "expectSuccess", G_TYPE_BOOLEAN, FALSE, NULL);
}

void test_parseLogLevel(gconstpointer data)
{
	string level = gcut_data_get_string(data, "level");
	const bool expectSuccess = gcut_data_get_string(data, "expectSuccess");

	TentativeEnvVariable tentativeEnv(Logger::LEVEL_ENV_VAR_NAME);
	CommandArgHelper cmds;
	cmds << "--log-level";
	cmds << level.c_str();
	cppcut_assert_equal(expectSuccess, cmds.activate());
	if (!expectSuccess)
		return;
	errno = 0;
	char *currEnv = getenv(tentativeEnv.getEnvVarName().c_str());
	cut_assert_errno();
	cppcut_assert_not_null(currEnv);
	cppcut_assert_equal(level, string(currEnv));
}

} // namespace testConfigManager
