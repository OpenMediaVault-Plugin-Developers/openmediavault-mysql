<?php

/**
 * Copyright (C) 2010-2012 Ian Moore <imooreyahoo@gmail.com>
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/notify.inc";
require_once "openmediavault/rpcservice.inc";

class OMVRpcServiceMySql extends OMVRpcServiceAbstract
{
    private $mysqldumpConfigurationFile = "/etc/openmediavault/mysqldump.cnf";
    private $mysqlConfigurationFile = "/etc/openmediavault/mysql.cnf";

    private function getEventMessagePath()
    {
        return "org.openmediavault.services.mysql";
    }

    private function getXpath()
    {
        return "/config/services/mysql";
    }

    /**
     * Get the name of the RPC service.
     *
     * @return string
     */
    public function getName()
    {
        return "MySQL";
    }

    /**
     * Initialize the RPC service.
     *
     * @return void
     */
    public function initialize()
    {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod("downloadBackup");
        $this->registerMethod("dumpDatabaseToSharedFolder");
        $this->registerMethod("uploadBackup");
        $this->registerMethod("resetPassword");
    }

    /**
     * Get main settings.
     *
     * @param array $params
     * @param array $context
     * @return array
     *
     * @throws OMVException
     */
    public function getSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Get configuration object
        $object = $xmlConfig->get($this->getXpath());

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXpath()
            );
        }

        // Modify result data
        $object["enable"] = intval($object["enable"]);
        $object["port"] = intval($object["port"]);
        $object["enable_networking"] = boolval($object["enable_networking"]);
        $object["disable_aio"] = boolval($object["disable_aio"]);
        $object["enable_management_site"] = boolval($object["enable_management_site"]);

        return $object;
    }

    /**
     * Set main settings.
     *
     * @param array $params
     * @param array $context
     * @return array
     *
     * @throws OMVException
     */
    public function setSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "enable": { "type": "boolean" },
                    "enable_networking": { "type": "boolean" },
                    "port": { "type": "integer" },
                    "bind_address": { "type": "string" },
                    "disable_aio": { "type": "boolean" },
                    "extra_options": { "type": "string" },
                    "enable_management_site": { "type": "boolean" }
                }
            }'
        );

        // Prepare configuration data.
        $object = array(
            "enable" => boolval($params["enable"]),
            "enable_networking" => boolval($params["enable_networking"]),
            "port" => intval($params["port"]),
            "bind_address" => $params["bind_address"],
            "disable_aio" => boolval($params["disable_aio"]),
            "extra_options" => trim($params["extra_options"]),
            "enable_management_site" => (
                boolval($params["enable_management_site"]) &&
                boolval($params["enable"])
            ),
        );

        // Set configuration object.
        if ($xmlConfig->replace($this->getXpath(), $object) === false) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXpath()
            );
        }

        // Notify configuration changes.
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            $this->getEventMessagePath(),
            $object
        );
    }

    /**
     * Download a backup of the database.
     *
     * @param array $params
     * @param array $context
     * @return array
     */
    public function downloadBackup($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        $sqlDump = tempnam(sys_get_temp_dir(), "mysql");
        chmod($sqlDump, 644);

        try {
            $this->dumpDatabase($sqlDump, $this->mysqldumpConfigurationFile);
        } catch (Exception $e) {
            // Clean up files.
            unlink($sqlDump);

            throw $e;
        }

        $now = new DateTime();
        $time = $now->format(DateTime::ISO8601);

        return array(
            "contenttype" => "application/sql",
            "filename" => "mysql-$time.sql",
            "filepath" => $sqlDump,
        );
    }

    /**
     * Dump the database to a shared folder.
     *
     * @param array $params
     * @param array $context
     * @return void
     *
     * @throws Exception
     */
    public function dumpDatabaseToSharedFolder($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "uuid": {' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . '}
                }
            }'
        );

        $shareMgmt = &OMVRpcServiceMgr::getInstance()->getService("ShareMgmt");
        $directory = $shareMgmt->callMethod("getPath", $params, $this->getAdminContext());

        // Construct the new filename.
        $now = new DateTime();
        $time = $now->format(DateTime::ISO8601);
        $sqlDump = $directory . DIRECTORY_SEPARATOR . "mysql-" . $time . ".sql";

        // Make sure we're not overwriting existing files.
        if (file_exists($sqlDump)) {
            throw new Exception("A file in the given location already exists.");
        }

        $this->dumpDatabase($sqlDump, $this->mysqldumpConfigurationFile);
    }

    /**
     * Upload and restore a backup of the database.
     *
     * @param array $params
     * @param array $context
     * @return void
     */
    public function uploadBackup($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "filename": { "type": "string" },
                    "filepath": { "type": "string" },
                    "password": { "type": "string" }
                }
            }'
        );

        $this->createRootPasswordConfigurationFile(
            $this->mysqlConfigurationFile,
            $params["password"],
            false
        );

        $this->restoreDatabaseDump(
            $params["filepath"],
            $this->mysqlConfigurationFile
        );

        unlink($this->mysqlConfigurationFile);
    }

    /**
     * Reset root password.
     *
     * @param array $params
     * @param array $context
     * @return void
     *
     * @throws OMVException
     */
    public function resetPassword($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "password": { "type": "string" }
                }
            }'
        );

        // Create temporary password file for resetting password.
        $file = new SplFileObject(tempnam(sys_get_temp_dir(), "tmp"), "w+");

        $file->fwrite($params["password"] . PHP_EOL);

        $cmd = sprintf(
            "export LANG=C; omv-mysql-reset-password %s 2>&1",
            $file->getRealPath()
        );

        $exitStatus = $this->exec($cmd, $output);

        // Always remove the file containing the password after use.
        unlink($file->getRealPath());

        if ($exitStatus !== 0) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode(PHP_EOL, $output)
            );
        }

        $this->createRootPasswordConfigurationFile(
            $this->mysqldumpConfigurationFile,
            $params["password"]
        );
    }

    /**
     * Creates a configuration file for MySQL containing the root password.
     * Used for things like automatic backup.
     *
     * @param string $file Where to store the configuration file.
     * @param string $password
     * @param bool $mysqlDump Wether to create a file for mysqldump or MySQL.
     * Defaults to true for mysqldump otherwise MySQL.
     * @return void
     */
    private function createRootPasswordConfigurationFile($path, $password, $mysqlDump = true)
    {
        $configGroup = "mysqldump";
        $file = new SplFileObject($path, "w+");

        // Make sure only the owner can read the file.
        chmod($file->getRealPath(), 0600);

        if (!$mysqlDump) {
            $configGroup = "mysql";
        }

        $file->fwrite("[$configGroup]" . PHP_EOL);
        $file->fwrite("user = root" . PHP_EOL);
        $file->fwrite("password = $password" . PHP_EOL);
    }

    /**
     * Dumps the database with the help of mysqldump.
     *
     * @param string $dumpFile Which file to dump the data in.
     * @param string $mysqldumpConfigurationFile Path to the configuration file
     * for mysqldump.
     * @return void
     *
     * @throws OMVException
     */
    private function dumpDatabase($dumpFile, $mysqldumpConfigurationFile)
    {
        $cmd = sprintf(
            "export LANG=C; mysqldump --defaults-extra-file=%s --all-databases --result-file=%s",
            $mysqldumpConfigurationFile,
            $dumpFile
        );

        if ($this->exec($cmd, $output) !== 0) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode(PHP_EOL, $output)
            );
        }
    }

    /**
     * Restore the database from a SQL dump.
     *
     * @param string $dumpFile Path to the SQL file.
     * @param string $mysqlConfigurationFile Path to the configuration file for
     * MySQL.
     * @return void
     *
     * @throws OMVException
     */
    private function restoreDatabaseDump($dumpFile, $mysqlConfigurationFile)
    {
        $cmd = sprintf(
            "export LANG=C; mysql --defaults-extra-file=%s < %s",
            $mysqlConfigurationFile,
            $dumpFile
        );

        if ($this->exec($cmd, $output) !== 0) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode(PHP_EOL, $output)
            );
        }
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceMySql());
