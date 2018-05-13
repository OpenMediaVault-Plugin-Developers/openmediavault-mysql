/**
 * Copyright (C) 2015-2018 OpenMediaVault Plugin Developers
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
// require("js/omv/module/admin/system/cron/Cron.js")

Ext.define('OMV.module.admin.service.mysql.ScheduledBackup', {
    extend: 'OMV.module.admin.system.cron.Job',

    title: _('Create scheduled backup job'),
    height: 310,
    hideResetButton: true,

    initComponent: function() {
        this.callParent(arguments);

        var enable = this.findField('enable');
        var username = this.findField('username');
        var command = this.findField('command');
        var comment = this.findField('comment');

        enable.hide();
        username.hide();
        command.hide();
        comment.hide();

        enable.setValue(true);
        username.setValue('root');
        comment.setValue('MySQL - Scheduled backup.');
    },

    getFormItems: function() {
        var prependItems = [{
            xtype: 'sharedfoldercombo',
            name: 'sharedfolderref',
            fieldLabel: _('Location'),
            allowBlank: true,
            allowNone: false,
            submitValue: false,
            listeners: {
                scope: this,
                change: function(combo, newValue) {
                    var command = this.findField('command');

                    var jsonData = JSON.stringify({
                        uuid: newValue
                    });

                    command.setValue('omv-rpc MySql dumpDatabaseToSharedFolder \'' + jsonData + '\'');
                }
            }
        }];

        var items = Ext.Array.push(prependItems, this.callParent(arguments));

        return items;
    }
});
