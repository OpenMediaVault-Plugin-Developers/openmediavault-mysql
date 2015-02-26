/**
 * Copyright (C) 2010-2012 Ian Moore <imooreyahoo@gmail.com>
 * Copyright (C) 2013-2014 OpenMediaVault Plugin Developers
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
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/form/plugin/LinkedFields.js")
// require("js/omv/window/MessageBox.js")

Ext.define("OMV.module.admin.service.mysql.Settings", {
    extend : "OMV.workspace.form.Panel",
    uses   : [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    plugins: [{
        ptype        : "linkedfields",
        correlations : [{
            name : [
                "data.sharedfolderref"
            ],
            conditions : [{
                name  : "enable",
                value : true
            }],
            properties : [
                "!allowBlank",
                "!allowNone"
            ]
        },{
            name : [
                "port",
                "bind_address"
            ],
            conditions : [{
                name  : "enable_networking",
                value : false
            }],
            properties : [
                "readOnly",
                "allowBlank"
            ]
        },{
            name : [
                "show_tab"
            ],
            conditions : [{
                name  : "enable_management_site",
                value : true
            },{
                name  : "enable",
                value : true
            }],
            properties : [
                "enabled"
            ]
        },{
            name : [
                "reset_password"
            ],
            conditions : [{
                name  : "enable",
                value : false
            }],
            properties: [
                "disabled"
            ]
        },{
            conditions  : [
                { name : "enable", value : true },
                { name : "enable_management_site", value : true }
            ],
            properties : function(valid, field) {
                this.setButtonDisabled("management", !valid);
            }
        }]
    }],

    initComponent : function () {
        this.on("load", function() {
            var checked = this.findField("enable").checked;
            var show_tab = this.findField("show_tab").checked;
            var parent = this.up("tabpanel");

            if (!parent)
                return;

            var managementPanel = parent.down("panel[title=" + _("Management") + "]");

            if (managementPanel) {
                if (checked) {
                    managementPanel.enable();
                } else {
                    managementPanel.disable();
                }

                if (show_tab) {
                    managementPanel.tab.show();
                } else {
                    managementPanel.tab.hide();
                }
            }
        }, this);

        this.callParent(arguments);
    },

    rpcService   : "MySql",
    rpcGetMethod : "getSettings",
    rpcSetMethod : "setSettings",

    getButtonItems : function() {
        var me = this;
        var items = me.callParent(arguments);
        items.push({
            id       : me.getId() + "-management",
            xtype    : "button",
            text     : _("Launch Management Site"),
            icon     : "images/mysql.png",
            iconCls  : Ext.baseCSSPrefix + "btn-icon-16x16",
            disabled : true,
            scope    : me,
            handler  : function() {
                window.open("/mysql/", "_blank");
            }
        });
        return items;
    },

    getFormItems : function() {
        return [{
            xtype    : "fieldset",
            title    : "General settings",
            defaults : {
                labelSeparator : ""
            },
            items : [{
                xtype      : "checkbox",
                name       : "enable",
                fieldLabel : _("Enable"),
                checked    : false
            },{
                xtype      : "checkbox",
                name       : "enable_networking",
                fieldLabel : _("Enable networking"),
                checked    : false
            },{
                xtype         : "numberfield",
                name          : "port",
                fieldLabel    : _("Port"),
                vtype         : "port",
                minValue      : 0,
                maxValue      : 65535,
                allowDecimals : false,
                allowNegative : false,
                allowBlank    : false,
                value         : 3306,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("Port to listen on.")
                }]
            },{
                xtype      : "textfield",
                name       : "bind_address",
                fieldLabel : _("Bind address"),
                vtype      : "IPv4Net",
                allowBlank : false,
                value      : "127.0.0.1",
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("IP address to listen on. Use 0.0.0.0 for all host IPs.")
                }]
            },{
                xtype      : "sharedfoldercombo",
                name       : "data.sharedfolderref",
                fieldLabel : _("Data directory"),
                allowBlank : true,
                allowNone  : true,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("The location where MySQL stores its data.") + " " +
                            _("It should be noted that the shared folder should be empty when applying the configuration and should not be altered with additional files. The reason for this is that the plugin will copy all files to the shared folder and remove them from the old data location.")
                }]
            },{
                xtype      : "checkbox",
                name       : "disable_aio",
                fieldLabel : _("Disable AIO"),
                checked    : false,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("Check this box if database is stored on ZFS filesystem.") 
                }]
            },{
                xtype      : "textarea",
                name       : "extra_options",
                fieldLabel : _("Extra options"),
                allowBlank : true,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("Extra options for [mysqld] section of MySQL configuration.")
                }]
            }]
        },{
            xtype    : "fieldset",
            title    : _("Reset MySQL root password"),
            defaults : {
                labelSeparator : ""
            },
            items : [{
                xtype       : "passwordfield",
                name        : "root_password",
                fieldLabel  : _("Password"),
                allowBlank  : true,
                submitValue : false
            },{
                xtype   : "button",
                name    : "reset_password",
                text    : _("Reset Password"),
                scope   : this,
                handler : Ext.Function.bind(this.doResetPassword, this, [ this ]),
                margin  : "5 0 8 0"
            }]
        },{
            xtype    : "fieldset",
            title    : _("SQL management site"),
            defaults : {
                labelSeparator : ""
            },
            items : [{
                xtype      : "checkbox",
                name       : "enable_management_site",
                fieldLabel : _("Enable"),
                boxLabel   : _("SQL management site."),
                checked    : false,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("The SQL web interface can be accessed <a href='/mysql/' target='_blank'>here</a>.") + " " +
                            _("For more advanced usage try: ") + "<a href='http://www.mysql.com/products/workbench/'>" + _("MySQL Workbench") + "</a>"
                }]
            },{
                xtype      : "checkbox",
                name       : "show_tab",
                fieldLabel : _("Enable"),
                boxLabel   : _("Show tab containing Management frame."),
                checked    : false
            }]
        }];
    },

    doResetPassword : function() {
        OMV.MessageBox.show({
            title   : _("Confirmation"),
            msg     : _("Are you sure you want to reset the root password?"),
            buttons : Ext.Msg.YESNO,
            fn      : function(answer) {
                if (answer !== "yes") {
                    return;
                }

                var rootPassword = this.getForm().findField("root_password").getValue();

                OMV.MessageBox.wait(null, _("Resetting MySQL root password."));

                OMV.Rpc.request({
                    scope       : this,
                    relayErrors : false,
                    rpcData     : {
                        service : "MySql",
                        method  : "resetPassword",
                        params  : {
                            root_password : rootPassword
                        }
                    },
                    success : function(id, success, response) {
                        this.doReload();
                        OMV.MessageBox.hide();
                    }
                });
            },
            icon  : Ext.Msg.QUESTION,
            scope : this
        });
    }
});

OMV.WorkspaceManager.registerPanel({
    id        : "settings",
    path      : "/service/mysql",
    text      : _("Settings"),
    position  : 10,
    className : "OMV.module.admin.service.mysql.Settings"
});
