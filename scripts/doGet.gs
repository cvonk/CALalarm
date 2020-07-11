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

    Logger.log("local time is", localTime(new Date()));

    const Namespace_OID = "{6ba7b812-9dad-11d1-80b4-00c04fd430c8}"
    var channelId = NameToUUID(Namespace_OID, e.parameter.devName);
    Logger.log('channelId =', channelId);

    var pushId = e.parameter.pushId;
    Logger.log('pushId =', pushId);

    disablePushNotifications(channelId, pushId);
    pushId = enablePushNotifications(channelId);

    var cal = CalendarApp.getCalendarById(email);
    if (cal == undefined) { return ContentService.createTextOutput("no access to calendar"); }

    var todayStart = new Date();
    const oneDay = 24 * 3600000;  // Convert to milliseconds for calculations
    todayStart.setHours(0, 0, 0); // Start at midnight today
    const now = new Date();
    const inHalfDay = new Date(now.getTime() + (oneDay / 2) - 1)
    var events = cal.getEvents(now, (new Date(now.getTime() + oneDay)));
    var eventStartRaw = [], eventEndRaw = [], raw = []

    for (var cc = 0; cc < events.length; cc++) {
        var event = events[cc];
        var dgb = events[cc].getTitle()
        switch (event.getMyStatus()) {
            case CalendarApp.GuestStatus.OWNER:
            case CalendarApp.GuestStatus.YES:
            case CalendarApp.GuestStatus.MAYBE: {
                // Filters on events that would actually show on the clock face if shown
                const allDayEvent = event.getStartTime() == undefined
                const didntEndYet = event.getEndTime() < inHalfDay
                const stillHasToStart = event.getStartTime() > now
                if (!allDayEvent) {
                    eventStartRaw.push(new Date(event.getStartTime().getTime() - 25200000))    //new Date(now.getTime() - 25200000)
                    eventEndRaw.push(new Date(event.getEndTime().getTime() - 25200000))
                    var qq = eventStartRaw.length - 1
                    if ((eventStartRaw[qq] != eventStartRaw[qq - 1]) && (eventEndRaw[qq] != eventEndRaw[qq - 1])) {
                        raw.push({ "start": eventStartRaw[qq], "stop": eventEndRaw[qq] })
                    }
                }
            }
            case CalendarApp.GuestStatus.NO:
            case CalendarApp.GuestStatus.INVITED:
                break;
        }
    }
    const correctedTime = new Date(now.getTime() - 25200000)
    const myJSON = JSON.stringify({ "time": correctedTime, "pushId": pushId, "events": raw });
    return ContentService.createTextOutput(myJSON).setMimeType(ContentService.MimeType.JSON);
}

function test() {
    var e = { 'parameter': { 'devName': 'dev123', 'pushId': '123' } };
    doGet(e);
}