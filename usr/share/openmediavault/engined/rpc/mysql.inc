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

 namespace OMV\Engined\Rpc;

use DateTime;
use Exception;
use SplFileObject;

use OMV\Config\Database;
use OMV\Engine\Notify\Dispatcher;
use OMV\Rpc\ServiceAbstract;
use OMV\Rpc\ServiceManager as RpcServiceManager;
use OMV\System\Process;

class MySql extends ServiceAbstract
{
    /** @var Database */
    private $database;

    /** @var string */
    private $dataModelPath = 'conf.service.mysql';

    /** @var Dispatcher */
    private $dispatcher;

    /** @var string */
    private $eventMessagePath = 'org.openmediavault.services.mysql';

    /** @var string */
    private $mysqldumpConfigurationFile = '/etc/openmediavault/mysqldump.cnf';

    /** @var string */
    private $mysqlConfigurationFile = '/etc/openmediavault/mysql.cnf';

    public function __construct(Database $database = null, Dispatcher $dispatcher = null)
    {
        $this->database = $database ? $database : Database::getInstance();
        $this->dispatcher = $dispatcher ? $dispatcher : Dispatcher::getInstance();
    }

    /**
     * Get the name of the RPC service.
     *
     * @return string
     */
    public function getName()
    {
        return 'MySQL';
    }

    /**
     * Initialize the RPC service.
     *
     * @return void
     */
    public function initialize()
    {
        $this->registerMethod('getSettings');
        $this->registerMethod('setSettings');
        $this->registerMethod('downloadBackup');
        $this->registerMethod('dumpDatabaseToSharedFolder');
        $this->registerMethod('uploadBackup');
        $this->registerMethod('resetPassword');
    }

    /**
     * Get main settings.
     *
     * @param array $params
     * @param array $context
     * @return array
     */
    public function getSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        $object = $this->database->get($this->dataModelPath);

        return $object->getAssoc();
    }

    /**
     * Set main settings.
     *
     * @param array $params
     * @param array $context
     * @return array
     */
    public function setSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.mysql.setsettings');

        $object = $this->database->get($this->dataModelPath);
        $object->setAssoc($params);

        $this->database->set($object);
        $this->dispatcher->notify(OMV_NOTIFY_MODIFY, $this->eventMessagePath, $object);

        return $object->getAssoc();
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
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        $sqlDump = tempnam(sys_get_temp_dir(), 'mysql');
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

        return [
            'contenttype' => 'application/sql',
            'filename' => sprintf('mysql-%s.sql', $time),
            'filepath' => $sqlDump,
        ];
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
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.mysql.dumpdatabasetosharedfolder');

        $shareMgmt = RpcServiceManager::getInstance()->getService('ShareMgmt');
        $directory = $shareMgmt->callMethod('getPath', $params, $this->getAdminContext());

        // Construct the new filename.
        $now = new DateTime();
        $time = $now->format(DateTime::ISO8601);
        $sqlDump = $directory . DIRECTORY_SEPARATOR . 'mysql-' . $time . '.sql';

        // Make sure we're not overwriting existing files.
        if (file_exists($sqlDump)) {
            throw new Exception('A file in the given location already exists.');
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
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.mysql.uploadbackup');

        $this->createRootPasswordConfigurationFile(
            $this->mysqlConfigurationFile,
            $params['password'],
            false
        );
        $this->restoreDatabaseDump($params['filepath'], $this->mysqlConfigurationFile);

        unlink($this->mysqlConfigurationFile);
    }

    /**
     * Reset root password.
     *
     * @param array $params
     * @param array $context
     * @return void
     */
    public function resetPassword($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.mysql.resetpassword');

        // Create temporary password file for resetting password.
        $file = new SplFileObject(tempnam(sys_get_temp_dir(), 'tmp'), 'w+');

        $file->fwrite($params['password'] . PHP_EOL);

        $cmd = new Process(
            'omv-mysql-reset-password',
            escapeshellarg($file->getRealPath())
        );
        $cmd->setRedirect2to1(true);

        try {
            $cmd->execute();
        } finally {
            // Always remove the file containing the password after use.
            unlink($file->getRealPath());
        }

        $this->createRootPasswordConfigurationFile(
            $this->mysqldumpConfigurationFile,
            $params['password']
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
        $configGroup = 'mysqldump';
        $file = new SplFileObject($path, 'w+');

        // Make sure only the owner can read the file.
        chmod($file->getRealPath(), 0600);

        if (!$mysqlDump) {
            $configGroup = 'mysql';
        }

        $file->fwrite('[' . $configGroup . ']' . PHP_EOL);
        $file->fwrite('user = root' . PHP_EOL);
        $file->fwrite('password = ' . $password . PHP_EOL);
    }

    /**
     * Dumps the database with the help of mysqldump.
     *
     * @param string $dumpFile Which file to dump the data in.
     * @param string $mysqldumpConfigurationFile Path to the configuration file
     * for mysqldump.
     * @return void
     */
    private function dumpDatabase($dumpFile, $mysqldumpConfigurationFile)
    {
        $cmd = new Process(
            'mysqldump',
            sprintf('--defaults-extra-file=%s', escapeshellarg($mysqldumpConfigurationFile)),
            '--all-databases',
            sprintf('--result-file=%s', $dumpFile)
        );

        $cmd->execute();
    }

    /**
     * Restore the database from a SQL dump.
     *
     * @param string $dumpFile Path to the SQL file.
     * @param string $mysqlConfigurationFile Path to the configuration file for
     * MySQL.
     * @return void
     */
    private function restoreDatabaseDump($dumpFile, $mysqlConfigurationFile)
    {
        $cmd = new Process(
            'mysql',
            sprintf('--defaults-extra-file=%s', escapeshellarg($mysqlConfigurationFile)),
            '<',
            escapeshellarg($dumpFile)
        );

        $cmd->execute();
    }
}
