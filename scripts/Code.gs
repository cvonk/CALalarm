//  Fetch all events to show on clock
//  Platform: Google Apps Script
//  (c) Copyright 2020, Sander Vonk

var timezone = Session.getScriptTimeZone();

function test() {
    var e = { 'parameter': { 'devName': 'calclock-2', 'pushId': null } };
    doGet(e);
}

function localTime(t) {
    return Utilities.formatDate(t, timezone, 'yyyy-MM-dd HH:mm:ss');
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
function nameToUUID(NamespaceUUID, Name) {

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

function enablePushNotifications(channelId, devName, email, duration) {

    const now = new Date();
    const oneMin = 60000;  // [msec]
    const resp = UrlFetchApp.fetch(
        'https://www.googleapis.com/calendar/v3/calendars/' + email + '/events/watch',
        {
            "method": "POST",
            "headers": {
                "Authorization": "Bearer " + ScriptApp.getOAuthToken(),
                "Content-Type": "application/json"
            },
            'payload': JSON.stringify({
                "id": channelId,
                "type": "web_hook",
                "address": "https://" + devName + ".coertvonk.com/api/push",
                'expiration': now.getTime() + duration * oneMin, // max is 1 hr
                "params": {
                    "ttl": (60 * duration).toString()            // max is 1 hr [sec]
                }
            }),
            "muteHttpExceptions": true  // false for dbg
        }
    );
    if (resp.getResponseCode() != 200) {
        // "channel id not unique" will dissapear as the channel expires
        Logger.log("enablePushNotifications, error", resp.getResponseCode(), resp.getContentText());
        return NaN;
    }
    return JSON.parse(resp).resourceId;
}

function doGet(e) {

    const namespaceOID = "{6ba7b812-9dad-11d1-80b4-00c04fd430c8}"
    var channelId = nameToUUID(namespaceOID, e.parameter.devName);
    disablePushNotifications(channelId, e.parameter.pushId);

    const email = Session.getEffectiveUser().getEmail();
    pushId = enablePushNotifications(channelId, e.parameter.devName, email, 60);

    var cal = CalendarApp.getCalendarById(email);
    if (!cal) {
      return ContentService.createTextOutput('no access to calendar (' + email + ')');
    }
    const now = new Date();
    const oneDay = 24 * 3600000;  // [msec]
    const events = cal.getEvents(now, (new Date(now.getTime() + oneDay)));

    var json = {
      "time": localTime(now),
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
                    json.events.push({
                        "start": localTime(event.getStartTime()),
                        "end": localTime(event.getEndTime())
                    });
                }
                break;
            default:
                break;
        }
    }
    return ContentService.createTextOutput(JSON.stringify(json)).setMimeType(ContentService.MimeType.JSON);
}

