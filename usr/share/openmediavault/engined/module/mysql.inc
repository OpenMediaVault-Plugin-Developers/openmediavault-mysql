<?php

/**
 * Copyright (C) 2010-2012 Ian Moore <imooreyahoo@gmail.com>
 * Copyright (C)      2013 OpenMediaVault Plugin Developers
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

require_once "openmediavault/module.inc";
require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/util.inc";
require_once "openmediavault/initscript.inc";

class OMVModuleMySQL extends OMVModuleServiceAbstract implements OMVINotifyListener, OMVIModuleServiceStatus {

    private function getXPath()
    {
        return '//services/mysql';
    }

	/**
	 * Get the module name.
	 */
	public function getName()
	{
		return "mysql";
	}

    /**
     * Get the module status
     */
    public function getStatus()
    {
        global $xmlConfig;

        $object = $xmlConfig->get($this->getXPath());

        if (is_null($object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $this->getXPath());

        $initd = new OMVSysVInitScript("mysql");

        return array(
            "name" => $this->getName(),
            "title" => gettext("MySQL"),
            "enabled" => boolval($object['enable']),
            "running" => ($initd->status() == 0)
        );
    }

	/**
	 * Create and update the configuration
	 */
	public function applyConfig()
	{
	    global $xmlConfig;

        $config = $xmlConfig->get($this->getXPath());

        if (is_null($config))
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $this->getXPath());

        $cmd = "sudo omv-mkconf mysql 2>&1";

        if (0 !== $this->exec($cmd, $output))
            throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                $cmd, implode("\n", $output));

        if (!empty($config['data-root']))
            $this->setupService($config);
    }

    /**
     * Initialize MySQL
     */
    private function setupService($conf, $econf)
    {
        try {
            $initd = new OMVSysVInitScript("mysql");

            // Stop MySQL if running
            try {
                $initd->stop();
            } catch (Exception $e) {
                // Ignored. Assume it's not running which is fine
            }

            // Ultimately files / folders must be readable / writable by mysql.
            $cmd = "sudo /bin/sh -c '[ -d {$conf['data-root']} ] || /bin/mkdir -p {$conf['data-root']}; chown mysql {$conf['data-root']}; chmod u+rwx {$conf['data-root']}'";
            if (0 !== $this->exec($cmd, $output))
                throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                    $cmd, implode("\n", $output));

            // If mysql database already exists, this was previously
            // initialized and there's nothing more to do
            $cmd = "sudo /bin/sh -c 'if [ -d {$conf['data-root']}/mysql ]; then echo OK; else echo NO; fi'";
            if (0 !== $this->exec($cmd, $output))
                throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                    $cmd, implode("\n", $output));

            if(trim($output[0]) == "OK")
                return;

            // Initialize default tables
            $cmd = "sudo mysql_install_db";
            if (0 !== $this->exec($cmd, $output))
                throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));

            // Start MySQL
            $initd->start();

            // TODO: Update this to mysqli with prepared statements and OO-approach
            // Assume blank root password
            // Access MySQL through existing functions in MySQLRpc
            $db = mysql_connect(null, 'root', '');
                if (!$db)
                    throw new Exception("Error connecting to MySQL: ". mysql_error());

            // Add debian user(s)
            exec("sudo php -r 'echo(serialize(parse_ini_file(\"/etc/mysql/debian.cnf\",true)));' 2>/dev/null", $out);
            foreach(unserialize($out[0]) as $s) {
                if(!$s['user'])
                    continue;

                $users[$s['user']] = $s['password'];
            }

            foreach($users as $key => $value) {
                $query = sprintf("GRANT ALL PRIVILEGES ON *.* TO '%s'@'localhost' IDENTIFIED BY '%s';",
                    mysql_real_escape_string($k, $db),
                    mysql_real_escape_string($v, $db));
                mysql_query($query, $db);
            }

            /**
             * This performs the same actions as /usr/bin/mysql_secure_installation
             */
            // Remove empty users
            $query = "DELETE FROM mysql.user where USER = ''";
            mysql_query($query, $db);

            // Drop test db
            $query = "DROP DATABASE IF EXISTS test";
            $stmt = $db->prepare($query);
            $db->execute($stmt);

            if (!empty($conf['password'])) {
                $query = sprintf("UPDATE mysql.user SET Password=PASSWORD('%s') WHERE User='root';",
                    mysql_real_escape_string($conf['password'], $db));
                mysql_query($query, $db);

                $query = "FLUSH PRIVILEGES;";
                mysql_query($query, $db);
            }

            mysql_close($db);
        } catch (Exception $e) {
            if ($db)
                mysql_close($db);
            throw $e;
        }
	}

    /**
     * Start the managed service
     */
    public function startService()
    {
        global $xmlConfig;

        $object = $xmlConfig->get($this->getXPath());

        if (is_null($object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $this->getXPath());

        if (boolval($object['enable']) === true) {
            $initd = new OMVSysVInitScript("mysql", true);
            $initd->exec();
        }
    }

    /**
     * Stop the managed service
     */
    public function stopService()
    {
        global $xmlConfig;

        $object = $xmlConfig->get($this->getXPath());

        if (is_null($object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $this->getXPath());

        if (boolval($object['enable']) === false) {
            $initd = new OMVSysVInitScript("mysql", false);
            $initd->exec();
        }
    }

	/**
	 * Bind listeners.
	 */
	public function bindListeners(OMVNotifyDispatcher $dispatcher) {
	    $dispatcher->addListener(
            OMV_NOTIFY_MODIFY,
            "org.openmediavault.services.mysql",
            array($this, "setDirty"));
	}
}

// Register module
$moduleMgr = &OMVModuleMgr::getInstance();
$moduleMgr->registerModule(new OMVModuleMySQL());
