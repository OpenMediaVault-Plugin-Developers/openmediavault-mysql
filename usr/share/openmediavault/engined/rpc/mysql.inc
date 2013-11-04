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

require_once "openmediavault/object.inc";
require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/util.inc";
require_once "openmediavault/rpcservice.inc";
require_once "openmediavault/notify.inc";

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
        $this->registerMethod('resetPassword');
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

        // Get actual mysql status
        $initd = new OMVSysVInitScript("mysql");

        // Modify result data
        $object['enable']                 = ($initd->status() == 0);
        $object['port']                   = intval($object['port']);
        $object['enable-networking']      = boolval($object['enable-networking']);
        $object['enable-management-site'] = boolval($object['enable-management-site']);
        $object['showtab']                = boolval($object['showtab']);

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
                "enable-management-site" : { "type" : "boolean" },
                "showtab"                : { "type" : "boolean" }
            }
        }');

        $xpath = sprintf("//system/fstab/mntent[uuid='%s']", $params['mntentref']);
        $deviceDir = $xmlConfig->get($xpath);
        $deviceDir = $deviceDir['dir'];

        $dataRoot = "{$deviceDir}/mysql-data";
        $params['data-root'] = $dataRoot;

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
            "mntentref"         => $params['mntentref'],
            "data-root"         => $params['data-root'],
            "showtab"           => array_boolval($params, 'showtab'),
        );

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXPath(), $object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED, $this->getXPath());

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_MODIFY, "org.openmediavault.services.mysql", $object);
    }

    public function resetPassword($params, $context) {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array( "role" => OMV_ROLE_ADMINISTRATOR ));

        $xpath = "//services/mysql";
        $object = $xmlConfig->get($xpath);
        if(is_null($object)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);
        }

        if(FALSE === boolval($object['enable']))
            return;

        $cmd = "sudo /usr/local/bin/mysql-password 2>&1";
        if(0 !== $this->exec($cmd, $output)) {
            throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
        }
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceMySQL());

?>