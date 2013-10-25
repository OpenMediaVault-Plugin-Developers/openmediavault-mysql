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

require_once ("openmediavault/object.inc");
require_once ("openmediavault/error.inc");
require_once ("openmediavault/util.inc");
require_once ("openmediavault/rpc.inc");
require_once ("openmediavault/notify.inc");

class OMVRpcServiceMySQL extends OMVRpcServiceAbstract
{
    private function getXPath()
    {
        return "//services/mysql";
    }

    public function getName()
    {
        return 'MySQL';
    }

    public function initialize()
    {
        $this->registerMethod('getSettings');
        $this->registerMethod('setSettings');
    }

    /**
     * Get main settings
     */
    function getSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Get configuration object
        $object = $xmlConfig->get($this->getXPath());
        if (is_null($object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPath());

        // Get actual mysql configuration and status
        $mySqlConf = $this->getMySqlConfig();
        $initd = new OMVSysVInitScript("mysql");
        $object['enable'] =

        // Modify result data
        $object['enable']                 = ($initd->status() == 0);
        $object['port']                   = intval($object['port']);
        $object['enable-networking']      = boolval($object['enable-networking']);
        $object['enable-management-site'] = boolval($object['enable-management-site']);

        if(!empty($object['password'])) {
            unset($object['password']);
            $object['nopasswordwarning'] = false;
        } else {
            $object['nopasswordwarning'] = true;
        }

        return $object;
    }

    /**
     * Set main settings
     */
    function setSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "enable"                 : { "type" : "boolean" },
                "enable-networking"      : { "type" : "boolean" },
                "port"                   : { "type" : "integer" },
                "bindaddress"            : { "type" : "string" },
                "extraoptions"           : { "type" : "string" },
                "mntentref"              : { "type" : "string" },
                "data-root"              : { "type" : "string" },
                "password"               : { "type" : "string" },
                "password-current"       : { "type" : "string" },
                "force-password-reset"   : { "type" : "boolean" },
                "enable-management-site" : { "type" : "boolean" }
            }
        }');

        $xpath = sprintf("//system/fstab/mntent[uuid='%s']", $params['mntentref']);
        $deviceDir = $xmlConfig->get($xpath);
        $deviceDir = $deviceDir['dir'];

        $dataRoot = "{$deviceDir}/mysql-data";
        $params['data-root'] = $dataRoot;

        $oldConfig = $xmlConfig->get($this->getXPath());

        foreach($oldConfig as $key => $value) {
            if (!isset($params[$key]))
                $params[$key] = $value;
        }
        if ($oldConfig['data-root'] != $dataRoot) {

                // data-root will be changed, check for existing dbs
                if(!$oldConfig['data-root']) {
                    exec("sudo grep '^datadir\s' /etc/mysql/my.cnf /etc/mysql/conf.d/* ".
                        "| grep -v openmediavault-mysql.cnf | awk '{ print \$NF }' | tail -1", $myconf);

                    $oldConfig['data-root'] = $myconf[0];
                }

                // Find all directories here that are not named "mysql"
                // which is the mysql system DB that exists in every mysql isntall
                exec("sudo find {$oldConfig['data-root']} -maxdepth 1 -type d -not -name mysql", $out);
                if(count($out) && $out[0] == $oldConfig['data-root'])
                    array_shift($out);

                // Directory found.
                if(count($out)) {
                    throw new Exception("Refusing to change MySQL data root while ".
                        "databases exist in {$oldConfig['data-root']}. This may result in ".
                        "loss of data.");
                }

        }

        // If we're just setting the config object to the current
        // password then just make sure we set the config
        if(!empty($params['password-current']) && empty($oldConfig['password']) && empty($params['password'])) {
            $params['password'] = $oldConfig['password'] = $params['password-current'];
        }

        // Prepare configuration data
        $object = array(
            "enable"            => array_boolval($params, 'enable'),
            "enable-networking" => array_boolval($params, 'enable-networking'),
            "enable-management-site" => (
                array_boolval($params, 'enable-management-site') && array_boolval($params, 'enable')
            ),
            "extraoptions"      => (!empty($params['extraoptions']) ? $params['extraoptions'] : ''),
            "port"              => intval($params['port']),
            "bindaddress"       => $params['bindaddress'],
            "password"          => (
                (!empty($params['password-current']) || !empty($params['password']) || $params['force-password-reset']) ?
                    $params['password'] : $oldConfig['password']
            ),
            "mntentref"         => $params['mntentref'],
            "data-root"         => $params['data-root'],
        );

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXPath(), $object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED, $this->getXPath());

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_MODIFY, "org.openmediavault.services.mysql", $object);

        // Set or change root password
        if($object['enable'] && (
                // Different passwords?
                ($oldConfig['password'] != $object['password']) ||

                // "force-password-reset" is selected and password was sent
                (array_boolval($params, 'force-password-reset') && strlen(trim($object['password'])) )
             )
         ) {
            try {
                if(empty($params['password-current']))
                    $params['password-current'] = '';

                // "Normal" MySQL root password change
                if(!array_boolval($params, 'force-password-reset')) {
                    $db = mysql_connect(null, 'root', $params['password-current']);
                    if (!$db)
                        throw new Exception("Error connecting to MySQL: ". mysql_error());

                    // TODO: Update this to mysqli with prepared statements and OO-approach
                    $query = sprintf("UPDATE mysql.user SET Password=PASSWORD('%s') WHERE User='root';",
                        mysql_real_escape_string($params['password'], $db));
                    mysql_query($query, $db);

                    $query = "FLUSH PRIVILEGES;";
                    mysql_query($query, $db);

                    mysql_close($db);
                // Forced MySQL root password change
                } else {
                    $cmd = "sudo omv-mkconf mysql-password 2>&1";
                    if ($this->exec($cmd, $output) !== 0)
                        throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
                }
            } catch (Exception $e) {
                // Failed to change password, change config back to old value
                $object['password'] = $oldConfig['password'];

                // Set configuration object
                if (false === $xmlConfig->replace($this->getXPath(), $object))
                    throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED, $this->getXPath());

                // Re-raise error
                throw new Exception($e->getMessage());
            }
        }
    }

    /**
    * Get the default MySQL configuration settings
    */
    private function getMySqlConfig()
    {
        exec('sudo /usr/sbin/mysqld --print-defaults | tail -1 | tr " " "\n" | sed -e \'s/^--//\'',
            $out);

        $defaults = array();
        foreach($out as $o) {
            if(!$o)
                continue;

            $set = explode('=',$o);

            if(count($set) == 1)
                $set[1] = true;

            $defaults[$set[0]] = $set[1];
        }

        return $defaults;
    }

}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceMySQL());
