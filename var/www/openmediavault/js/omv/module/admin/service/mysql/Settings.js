/**
 * Copyright (C) 2010-2012 Ian Moore <imooreyahoo@gmail.com>
 * Copyright (C) 2013-2017 OpenMediaVault Plugin Developers
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

// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/form/Panel.js")
// require("js/omv/window/MessageBox.js")
// require("js/omv/form/plugin/LinkedFields.js")
// require("js/omv/module/admin/service/mysql/Password.js")
// require("js/omv/module/admin/service/mysql/ScheduledBackup.js")

Ext.define('OMV.module.admin.service.mysql.Settings', {
    extend: 'OMV.workspace.form.Panel',
    requires: [
        'OMV.module.admin.service.mysql.Password',
        'OMV.module.admin.service.mysql.ScheduledBackup'
    ],

    plugins: [{
        ptype: 'linkedfields',
        correlations: [{
            conditions: [{
                name: 'enable',
                value: true
            }],
            properties: function(valid, field) {
                this.setButtonDisabled('reset-password', !valid);
                this.setButtonDisabled('backup', !valid);
                this.setButtonDisabled('scheduled-backup', !valid);
                this.setButtonDisabled('restore', !valid);
            }
        }, {
            name: [
                'port',
                'bind_address'
            ],
            conditions: [{
                name: 'enable_networking',
                value: false
            }],
            properties: [
                'readOnly',
                'allowBlank'
            ]
        }, {
            conditions: [{
                name: 'enable',
                value: true
            }, {
                name: 'enable_management_site',
                value: true
            }],
            properties: function(valid, field) {
                this.setButtonDisabled('show', !valid);
            }
        }]
    }],

    rpcService: 'MySql',
    rpcGetMethod: 'getSettings',
    rpcSetMethod: 'setSettings',

    getButtonItems: function() {
        var items = this.callParent(arguments);

        items.push({
            id: this.getId() + '-show',
            xtype: 'button',
            text: _('Show'),
            icon: 'images/mysql.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            disabled: true,
            scope: this,
            handler: function() {
                window.open('/mysql/', '_blank');
            }
        }, {
            id: this.getId() + '-reset-password',
            xtype: 'button',
            text: _('Reset Password'),
            icon: 'images/wrench.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            scope: this,
            handler: Ext.Function.bind(this.onResetPasswordButton, this)
        }, {
            id: this.getId() + '-backup',
            xtype: 'button',
            text: _('Backup'),
            icon: 'images/wrench.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            scope: this,
            handler: Ext.Function.bind(this.onBackupButton, this)
        }, {
            id: this.getId() + '-scheduled-backup',
            xtype: 'button',
            text: _('Scheduled backup'),
            icon: 'images/wrench.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            scope: this,
            handler: Ext.Function.bind(this.onScheduledBackupButton, this)
        }, {
            id: this.getId() + '-restore',
            xtype: 'button',
            text: _('Restore'),
            icon: 'images/wrench.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            scope: this,
            handler: Ext.Function.bind(this.onRestoreButton, this),
        });

        return items;
    },

    getFormItems: function() {
        return [{
            xtype: 'fieldset',
            title: 'General settings',
            defaults: {
                labelSeparator: ''
            },
            items: [{
                xtype: 'checkbox',
                name: 'enable',
                fieldLabel: _('Enable'),
                checked: false
            }, {
                xtype: 'checkbox',
                name: 'enable_networking',
                fieldLabel: _('Enable networking'),
                checked: false
            }, {
                xtype: 'numberfield',
                name: 'port',
                fieldLabel: _('Port'),
                vtype: 'port',
                minValue: 0,
                maxValue: 65535,
                allowDecimals: false,
                allowNegative: false,
                allowBlank: false,
                value: 3306,
                plugins: [{
                    ptype: 'fieldinfo',
                    text: _('Port to listen on.')
                }]
            }, {
                xtype: 'textfield',
                name: 'bind_address',
                fieldLabel: _('Bind address'),
                vtype: 'IPv4Net',
                allowBlank: false,
                value: '127.0.0.1',
                plugins: [{
                    ptype: 'fieldinfo',
                    text: _('IP address to listen on. Use 0.0.0.0 for all host IPs.')
                }]
            }, {
                xtype: 'checkbox',
                name: 'disable_aio',
                fieldLabel: _('Disable AIO'),
                checked: false,
                plugins: [{
                    ptype: 'fieldinfo',
                    text: _('Check this box if database is stored on ZFS filesystem.')
                }]
            }, {
                xtype: 'textarea',
                name: 'extra_options',
                fieldLabel: _('Extra options'),
                allowBlank: true,
                plugins: [{
                    ptype: 'fieldinfo',
                    text: _('Extra options for [mysqld] section of MySQL configuration.')
                }]
            }]
        }, {
            xtype: 'fieldset',
            title: _('SQL management site'),
            defaults: {
                labelSeparator: ''
            },
            items: [{
                xtype: 'checkbox',
                name: 'enable_management_site',
                fieldLabel: _('Enable'),
                boxLabel: _('SQL management site.'),
                checked: false,
                plugins: [{
                    ptype: 'fieldinfo',
                    text: _('Click Reset Password to create omvadmin user and set password to allow management site login.') +
                          _('omvadmin is also used for backups/restores.')
                }]
            }]
        }];
    },

    onResetPasswordButton: function() {
        Ext.create('OMV.module.admin.service.mysql.Password', {
            title: _('Reset MySQL omvadmin password.'),
            mode: 'remote',
            rpcService: 'MySql',
            rpcSetMethod: 'resetPassword'
        }).show();
    },

    onBackupButton: function() {
        OMV.Download.request('MySql', 'downloadBackup');
    },

    onScheduledBackupButton: function() {
        Ext.create('OMV.module.admin.service.mysql.ScheduledBackup', {
            uuid: OMV.UUID_UNDEFINED
        }).show();
    },

    onRestoreButton: function() {
        Ext.create('OMV.module.admin.service.mysql.Password', {
            title: _('Provide MySQL omvadmin password to restore dump.'),
            listeners: {
                scope: this,
                submit: function(wnd, params) {
                    Ext.create('OMV.window.Upload', {
                        title: _('Upload backup'),
                        service: 'MySql',
                        method: 'uploadBackup',
                        params: params,
                        listeners: {
                            scope: this,
                            success: function(wnd, response) {
                                OMV.MessageBox.info(_('Restored backup'), _('Backup was successfully restored.'));
                            }
                        }
                    }).show();
                }
            }
        }).show();
    }
});

OMV.WorkspaceManager.registerPanel({
    id: 'settings',
    path: '/service/mysql',
    text: _('Settings'),
    position: 10,
    className: 'OMV.module.admin.service.mysql.Settings'
});
