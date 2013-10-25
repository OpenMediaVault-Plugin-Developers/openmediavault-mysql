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
        }]
    }],

    rpcService   : "MySql",
    rpcGetMethod : "getSettings",
    rpcSetMethod : "setSettings",

    initComponent : function() {
        var me = this;

        me.on('load', function () {
            var noPasswordWarning = me.down('#no-password-warning');
            me.nopasswordwarning ? noPasswordWarning.show() : noPasswordWarning.hide();
        });

        me.callParent(arguments);
    },

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
                    text  : _("Warning: If Your MySQL data volume is set to use a non-data volume it may result in loss of data in the event of a disk failure. It is recommmended that you change this before using MySQL. Once databases exist, you will be unable to choose the location of MySQL data through this interface.")
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
            title    : _("Change MySQL root password"),
            defaults : {
                labelSeparator : ""
            },
            items : [{
                border : false,
                itemId : "no-password-warning",
                html   : "<p>Warning: Either the MySQL root password is not set, or it is not known to OpenMediaVault.</p>"
            },{
                xtype      : "passwordfield",
                name       : "password-current",
                fieldLabel : _("Current password"),
                allowBlank : true,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("If you have just installed this plugin and had not previously used MySQL, you should leave Current password blank and use the New password fields to set the MySQL root password.<br />"
                            + "If you have an existing MySQL installation simply tell OpenMediaVault what the current root password is by entering it in the Current password field.")
                }]
            },{
                xtype      : "passwordfield",
                name       : "password",
                fieldLabel : _("New password"),
                allowBlank : true,
                validator  : function() {
                    var password        = me.findField("password");
                    var passwordConfirm = me.findField("password-confirm");

                    return password.value === passwordConfirm.value;
                }
            },{
                xtype       : "passwordfield",
                name        : "password-confirm",
                fieldLabel  : _("Confirm new password"),
                submitValue : false,
                allowBlank  : true,
                validator   : function() {
                    var password        = me.findField("password");
                    var passwordConfirm = me.findField("password-confirm");

                    return password.value === passwordConfirm.value;
                }
            },{
                xtype          : "button",
                fieldLabel     : _("Generate password"),
                text           : _("Generate"),
                name           : "generate-password",
                handler        : function() {
                    var passwordFields = ['password', 'password-confirm'];
                    var newPassword = me.generatePassword();

                    Ext.each(passwordFields, function(p) {
                        var field = me.findField(p);
                        if (field) {
                            field.setValue(newPassword);
                            if (field.inputEl.dom.type === "password")
                                field.onTriggerClick();
                        }
                    });
                }
            },{
                xtype      : "checkbox",
                name       : "force-password-reset",
                fieldLabel : _("Force password reset"),
                value      : false,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("This will cause a restart of MySQL and should only be required in rare cases.")
                }]
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
                fieldLabel : _("Enable SQL management site"),
                checked    : false,
                plugins    : [{
                    ptype : "fieldinfo",
                    text  : _("For more advanced usage try <a href='http://www.mysql.com/products/workbench/'>MySQL Workbench</a>")
                }]
            },{
                xtype      : "button",
                name       : "launch-management-site",
                text       : _("Launch management site"),
                disabled   : true,
                handler    : function() {
                    window.open("/sqlbuddy/");
                }
            }]
        }];
    },

    generatePassword : function() {
        var pwchars = "abcdefhjmnpqrstuvwxyz23456789ABCDEFGHJKLMNPQRSTUVWYXZ";
        var passwordlength = 16;
        var password = '';

        for (i = 0; i < passwordlength; i++) {
            password += pwchars.charAt(Math.floor( Math.random() * pwchars.length));
        }

        return password;
    }
});

OMV.WorkspaceManager.registerPanel({
    id        : "settings",
    path      : "/service/mysql",
    text      : _("Settings"),
    position  : 10,
    className : "OMV.module.admin.service.mysql.Settings"
});
