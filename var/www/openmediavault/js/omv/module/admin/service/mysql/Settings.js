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
                "port",
                "bindaddress"
            ],
            conditions : [{
                name  : "enable-networking",
                value : false
            }],
            properties : [
                "readOnly",
                "allowBlank"
            ]
        },{
            name : [
                "launch-management-site"
            ],
            conditions : [{
                name  : "enable-management-site",
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
                "resetpwd"
            ],
            conditions : [{
                name  : "enable",
                value : false
            }],
            properties: [
                "disabled"
            ]
        }]
    }],

    initComponent : function () {
        var me = this;

        me.on('load', function () {
            var checked = me.findField('enable').checked;
            var showtab = me.findField('showtab').checked;
            var parent = me.up('tabpanel');

            if (!parent)
                return;

            var managementPanel = parent.down('panel[title=' + _("Management") + ']');

            if (managementPanel) {
                checked ? managementPanel.enable() : managementPanel.disable();
                showtab ? managementPanel.tab.show() : managementPanel.tab.hide();
            }
        });

        me.callParent(arguments);
    },

    rpcService   : "MySql",
    rpcGetMethod : "getSettings",
    rpcSetMethod : "setSettings",

    getFormItems : function() {
        var me = this;

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
                name       : "enable-networking",
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
                name       : "bindaddress",
                fieldLabel : _("Bind address"),
                vtype      : "IPv4Net",
                allowBlank : false,
                value      : "127.0.0.1",
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("IP address to listen on. Use 0.0.0.0 for all host IPs.")
                }]
            },{
                xtype      : "textarea",
                name       : "extraoptions",
                fieldLabel : _("Extra options"),
                allowBlank : true,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("Extra options for [mysqld] section of MySQL configuration.")
                }]
            }]
        },{
            xtype    : "fieldset",
            title    : _("Data"),
            defaults : {
                labelSeparator : ""
            },
            items : [{
                xtype         : "combo",
                name          : "mntentref",
                fieldLabel    : _("SQL data volume"),
                emptyText     : _("Select a volume ..."),
                allowBlank    : false,
                allowNone     : false,
                editable      : false,
                triggerAction : "all",
                displayField  : "description",
                valueField    : "uuid",
                store         : Ext.create("OMV.data.Store", {
                    autoLoad : true,
                    model    : OMV.data.Model.createImplicit({
                        idProperty : "uuid",
                        fields     : [
                            { name : "uuid", type : "string" },
                            { name : "devicefile", type : "string" },
                            { name : "description", type : "string" }
                        ]
                    }),
                    proxy : {
                        type : "rpc",
                        rpcData : {
                            service : "ShareMgmt",
                            method  : "getCandidates"
                        },
                        appendSortParams : false
                    },
                    sorters : [{
                        direction : "ASC",
                        property  : "devicefile"
                    }]
                }),
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("Warning: If Your MySQL data volume is set to use a non-data volume it may result in loss of data in the event of a disk failure. It is recommmended that you change this before using MySQL.")
                }]
            },{
                xtype      : "textfield",
                name       : "data-root",
                fieldLabel : _("SQL data root"),
                allowNone  : true,
                readOnly   : true
            }]
        },{
            xtype    : "fieldset",
            title    : _("Reset MySQL root password"),
            defaults : {
                labelSeparator : ""
            },
            items : [{
                xtype   : "button",
                name    : "resetpwd",
                text    : _("Reset Password"),
                scope   : this,
                handler : function() {
                    OMV.MessageBox.show({
                        title   : _("Confirmation"),
                        msg     : _("Are you sure you want to reset the root password?"),
                        buttons : Ext.Msg.YESNO,
                        fn      : function(answer) {
                            if (answer !== "yes")
                               return;

                            OMV.MessageBox.wait(null, _("Resetting MySQL root password."));

                            OMV.Rpc.request({
                                scope       : me,
                                relayErrors : false,
                                rpcData     : {
                                    service : "MySql",
                                    method  : "resetPassword"
                                },
                                success : function(id, success, response) {
                                    me.doReload();
                                    OMV.MessageBox.hide();
                                }
                            });
                        },
                        scope : me,
                        icon  : Ext.Msg.QUESTION
                    });
                }
            },{
                border : false,
                html   : "<br />" + 
                         _("Password will reset to:") + "  openmediavault" + "<br /><br />" +
                         _("To change the password, use the management site and change root user on host localhost.") + "<br /><br />"
            }]
        },{
            xtype    : "fieldset",
            title    : _("SQL management site"),
            defaults : {
                labelSeparator : ""
            },
            items : [{
                xtype      : "checkbox",
                name       : "enable-management-site",
                fieldLabel : _("Enable"),
                boxLabel: _("SQL management site."),
                checked    : false,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("For more advanced usage try: ") + "<a href='http://www.mysql.com/products/workbench/'>" + _("MySQL Workbench") + "</a>"
                }]
            },{
                xtype: "checkbox",
                name: "showtab",
                fieldLabel: _("Enable"),
                boxLabel: _("Show tab containing Management frame."),
                checked: false
            },{
                xtype      : "button",
                name       : "launch-management-site",
                text       : _("Launch management site"),
                disabled   : true,
                handler    : function() {
                    window.open("/sqlbuddy/");
                }
            },{
                border: false,
                html: "</p>"                
            }]
        }];
    }
});

OMV.WorkspaceManager.registerPanel({
    id        : "settings",
    path      : "/service/mysql",
    text      : _("Settings"),
    position  : 10,
    className : "OMV.module.admin.service.mysql.Settings"
});
