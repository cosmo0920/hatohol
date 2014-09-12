/*
 * Copyright (C) 2013-2014 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

var TriggersView = function(userProfile) {
  var self = this;
  var rawData;

  self.reloadIntervalSeconds = 60;
  self.numRecordsPerPage = 50;

  // call the constructor of the super class
  HatoholMonitoringView.apply(this, [userProfile]);

  self.pager = new HatoholPager();
  self.userConfig = new HatoholUserConfig();
  start();

  function start() {
    var numRecordsPerPage;
    self.userConfig.get({
      itemNames:['num-triggers-per-page'],
      successCallback: function(conf) {
        self.numRecordsPerPage =
          self.userConfig.findOrDefault(conf, 'num-triggers-per-page',
                                        self.numRecordsPerPage);
        updatePager();
        setupCallbacks();
        load();
      },
      connectErrorCallback: function(XMLHttpRequest) {
        showXHRError(XMLHttpRequest);
      },
    });
  }

  function showXHRError(XMLHttpRequest) {
    var errorMsg = "Error: " + XMLHttpRequest.status + ": " +
      XMLHttpRequest.statusText;
    hatoholErrorMsgBox(errorMsg);
  }

  function saveConfig(items) {
    self.userConfig.store({
      items: items,
      successCallback: function() {
        // we just ignore it
      },
      connectErrorCallback: function(XMLHttpRequest) {
        showXHRError(XMLHttpRequest);
      },
    });
  }

  function updatePager() {
    self.pager.update({
      numTotalRecords: rawData ? rawData["totalNumberOfTriggers"] : -1,
      numRecordsPerPage: self.numRecordsPerPage,
      selectPageCallback: function(page) {
        load(page);
        if (self.pager.numRecordsPerPage != self.numRecordsPerPage) {
          self.numRecordsPerPage = self.pager.numRecordsPerPage;
          saveConfig({'num-triggers-per-page': self.numRecordsPerPage})
        }
      }
    });
  }

  var status_choices = [
    gettext("OK"),
    gettext("Problem"),
    gettext("Unknown")
  ];
  var severity_choices = [
    gettext("Not classified"),
    gettext("Information"),
    gettext("Warning"),
    gettext("Average"),
    gettext("High"),
    gettext("Disaster")
  ];

  function setupCallbacks() {
    $("#table").stupidtable();
    $("#table").bind('aftertablesort', function(event, data) {
      var th = $(this).find("th");
      th.find("i.sort").remove();
      var icon = data.direction === "asc" ? "up" : "down";
      th.eq(data.column).append("<i class='sort glyphicon glyphicon-arrow-" + icon +"'></i>");
    });

    self.setupHostQuerySelectorCallback(
      load, '#select-server', '#select-host-group', '#select-host');
    $("#select-severity, #select-status").change(function() {
      load();
    });
  }

  function setLoading(loading) {
    if (loading) {
      $("#select-severity").attr("disabled", "disabled");
      $("#select-status").attr("disabled", "disabled");
      $("#select-server").attr("disabled", "disabled");
      $("#select-hostgroup").attr("disabled", "disabled");
      $("#select-host").attr("disabled", "disabled");
    } else {
      $("#select-severity").removeAttr("disabled");
      $("#select-status").removeAttr("disabled");
      $("#select-server").removeAttr("disabled");
      if ($("#select-hostgroup option").length > 1)
        $("#select-hostgroup").removeAttr("disabled");
      if ($("#select-host option").length > 1)
        $("#select-host").removeAttr("disabled");
    }
  }

  function drawTableBody(replyData) {
    var serverName, hostName, clock, status, severity;
    var html, server, trigger;
    var x, serverId, hostId;

    html = "";
    for (x = 0; x < replyData["triggers"].length; ++x) {
      trigger    = replyData["triggers"][x];
      serverId   = trigger["serverId"];
      hostId     = trigger["hostId"];
      server     = replyData["servers"][serverId];
      serverName = getServerName(server, serverId);
      hostName   = getHostName(server, hostId);
      clock      = trigger["lastChangeTime"];
      status     = trigger["status"];
      severity   = trigger["severity"];

      html += "<tr><td>" + escapeHTML(serverName) + "</td>";
      html += "<td class='severity" + escapeHTML(severity) +
        "' data-sort-value='" + escapeHTML(severity) + "'>" +
        severity_choices[Number(severity)] + "</td>";
      html += "<td class='status" + escapeHTML(status) +
        "' data-sort-value='" + escapeHTML(status) + "'>" +
        status_choices[Number(status)] + "</td>";
      html += "<td data-sort-value='" + escapeHTML(clock) + "'>" +
        formatDate(clock) + "</td>";
      /* Not supported yet
      html += "<td>" + "unsupported" + "</td>";
      html += "<td>" + "unsupported" + "</td>";
      */
      html += "<td>" + escapeHTML(hostName) + "</td>";
      html += "<td>" + escapeHTML(trigger["brief"]) + "</td>";
      // Not supported yet
      //html += "<td>" + "unsupported" + "</td>";
      html += "</tr>";
    }

    return html;
  }

  function drawTableContents(data) {
    $("#table tbody").empty();
    $("#table tbody").append(drawTableBody(data));
  }

  function updateCore(reply) {
    rawData = reply;

    self.setServerFilterCandidates(rawData["servers"]);
    self.setHostgroupFilterCandidates(rawData["servers"]);
    self.setHostFilterCandidates(rawData["servers"]);

    drawTableContents(rawData);
    updatePager();
    setLoading(false);
    self.setAutoReload(load, self.reloadIntervalSeconds);
  }

  function getQuery(page) {
    if (isNaN(page))
      page = 0;
    var query = {
      minimumSeverity: $("#select-severity").val(),
      status:          $("#select-status").val(),
      limit:           self.pager.numRecordsPerPage,
      offset:          self.pager.numRecordsPerPage * page
    };
    self.addHostQuery(query);
    return 'trigger?' + $.param(query);
  };

  function load(page) {
    self.startConnection(getQuery(page), updateCore);
    setLoading(true);
    self.pager.update({ currentPage: isNaN(page) ? 0 : page });
  }
};

TriggersView.prototype = Object.create(HatoholMonitoringView.prototype);
TriggersView.prototype.constructor = TriggersView;
