//  Fetch all events to show on clock
//  Platform: Google Apps Script
//  (c) Copyright 2020, Sander Vonk

var timezone = Session.getScriptTimeZone();

function localTime(t) {
    return Utilities.formatDate(new Date(), timezone, 'yyyy-MM-dd HH:mm:ss');
}

function bytesToString(bytes) {
    var str = '';
    for (ii = 0; ii < bytes.length; ii++) {
        var byte = bytes[ii];
        if (byte < 0) byte += 256;
        var byteStr = byte.toString(16);
        if (byteStr.length == 1) {
            byteStr = '0' + byteStr;
        }
        if (ii == 4 || ii == 6 || ii == 8) {
            str += '-';
        }
        str += byteStr;
    }
    return str;
}

// https://stackoverflow.com/questions/10867405/generating-v5-uuid-what-is-name-and-namespace
function NameToUUID(NamespaceUUID, Name) {

    hash = Utilities.computeDigest(Utilities.DigestAlgorithm.SHA_1, NamespaceUUID + Name).slice(0, 14);
    hash[6] = (hash[6] & 0x0F) | 0x50;
    hash[8] = (hash[8] & 0x3F) | 0x80;
    return bytesToString(hash);
}

function disablePushNotifications(channelId, resourceId) {

    UrlFetchApp.fetch('https://www.googleapis.com/calendar/v3/channels/stop',
        {
            "method": "POST",
            "headers": {
                "Authorization": "Bearer " + ScriptApp.getOAuthToken(),
                "Content-Type": "application/json"
            },
            'payload': JSON.stringify({
                "id": channelId,
                "resourceId": resourceId
            }),
            "muteHttpExceptions": true
        });
}

function enablePushNotifications(channeId, email) {

    const resp = UrlFetchApp.fetch(
        'https://www.googleapis.com/calendar/v3/calendars/' + email + '/events/watch',
        {
            "method": "POST",
            "headers": {
                "Authorization": "Bearer " + ScriptApp.getOAuthToken(),
                "Content-Type": "application/json"
            },
            'payload': JSON.stringify({
                "id": channeId, //"01234567-89ab-cdef-0123456789ab",//,generate_uuid() your channel UUID
                "type": "web_hook",
                "address": "https://calendarclock.coertvonk.com/api/push",
                "params": {
                    "ttl": 320 // [sec]
                }
            }),
            "muteHttpExceptions": true
        }
    );
    if (resp.getResponseCode() != 200) {
        Logger.log("enablePushNotifications, error", resp.getResponseCode(), resp.getContentText());
        return NaN;
    }
    return JSON.parse(resp).resourceId;
}

function doGet(e) {

    const email = Session.getEffectiveUser().getEmail();

    var pushId = e.parameter.pushId;
    const Namespace_OID = "{6ba7b812-9dad-11d1-80b4-00c04fd430c8}"
    var channelId = NameToUUID(Namespace_OID, e.parameter.devName);

    disablePushNotifications(channelId, pushId);
    pushId = enablePushNotifications(channelId);

    var cal = CalendarApp.getCalendarById(email);
    if (!cal) {
      return ContentService.createTextOutput("no access to calendar");
    }
    const now = new Date();
    const oneDay = 24 * 3600000;  // [msec]
    const events = cal.getEvents(now, (new Date(now.getTime() + oneDay)));

    Logger.log("local time is", localTime(now));
    var json = {
      "time": new Date(now.getTime() - 25200000),
      "pushId": pushId,
      "events": [],
    };

    for (var cc = 0; cc < events.length; cc++) {
        var event = events[cc];
        const allDayEvent = event.getStartTime() == undefined;
        switch (event.getMyStatus()) {
            case CalendarApp.GuestStatus.OWNER:
            case CalendarApp.GuestStatus.YES:
            case CalendarApp.GuestStatus.MAYBE:
                if (!allDayEvent) {
                    const localStart = new Date(event.getStartTime().getTime() - 25200000);
                    const localEnd = new Date(event.getEndTime().getTime() - 25200000);
                    json.events.push({ "start": localStart, "stop": localEnd });
                }
                break;
            case CalendarApp.GuestStatus.NO:
            case CalendarApp.GuestStatus.INVITED:
                break;
        }
    }
    return ContentService.createTextOutput(JSON.stringify(json)).setMimeType(ContentService.MimeType.JSON);
}

function test() {
    var e = { 'parameter': { 'devName': 'dev123', 'pushId': null } };
    doGet(e);
}