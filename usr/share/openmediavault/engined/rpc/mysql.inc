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
        $this->registerMethod("resetPassword");
    }

    /**
     * Get main settings.
     *
     * @param array $params
     * @param array $context
     *
     * @return array
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
     *
     * @return array
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
        if (false === $xmlConfig->replace($this->getXpath(), $object)) {
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
     * Reset root password.
     *
     * @param array $params
     * @param array $context
     *
     * @return void
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
                    "root_password": { "type": "string" }
                }
            }'
        );

        // Create temporary password file for resetting password.
        $file = new SplFileObject(
            tempnam(sys_get_temp_dir(), "tmp"),
            "w+"
        );

        $file->fwrite($params["root_password"] . PHP_EOL);

        $cmd = sprintf(
            "export LANG=C; omv-mysql-reset-password %s 2>&1",
            $file->getRealPath()
        );

        $exitStatus = $this->exec($cmd, $output);

        // Always remove the file containing the password after use.
        unlink($file->getRealPath());

        if (0 !== $exitStatus) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode("\n", $output)
            );
        }

        $this->createRootPasswordConfigurationFile($params["root_password"]);
    }

    /**
     * Creates a configuration file for MySQL containing the root password.
     * Used for things like automatic backup.
     *
     * @param string $password
     *
     * @return void
     */
    private function createRootPasswordConfigurationFile($password)
    {
        $file = new SplFileObject("/root/.my.cnf", "w+");

        chmod($file->getRealPath(), 0600);

        $file->fwrite("[mysqldump]" . PHP_EOL);
        $file->fwrite("user = root" . PHP_EOL);
        $file->fwrite("password = $password" . PHP_EOL);
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceMySql());
