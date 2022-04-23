//  Fetch the the alarm event for CALalarm device
//  Platform: Google Apps Script
//  (c) Copyright 2020,2022, Sander Vonk

let timezone = Session.getScriptTimeZone();  // update in appsscript.json if needed

function test() {
    let e = { 'parameter': { 'devName': 'calalarm', 'pushId': null } };
    doGet(e);
}

function localTime(t) {
    return Utilities.formatDate(t, timezone, 'yyyy-MM-dd HH:mm:ss');
}

function bytesToString(bytes) {
    let str = '';
    for (ii = 0; ii < bytes.length; ii++) {
        let byte = bytes[ii];
        if (byte < 0) byte += 256;
        let byteStr = byte.toString(16);
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

// https://code.google.com/p/google-apps-script-issues/issues/detail?id=4433
function _alarmTime(event) {

    if (event == undefined ) {
      return undefined;
    }
    let reminders = event.getPopupReminders();  
    // known issue: method getPopupReminders() doesn't return anything if there is only one reminder
    if (reminders.length == 0) {
        reminder = 0;   
    }
    date = new Date(event.getStartTime().getTime()-reminders[0]*60*1000);
    return date;
}

function _findFirstAlarm(events) {

    let firstAlarmTime = undefined;
    let firstAlarmIdx = undefined;
    
    for (let ii = 0; ii < events.length; ii++) {
        let event=events[ii];    
        switch(event.getMyStatus()) {
            case CalendarApp.GuestStatus.OWNER:
            case CalendarApp.GuestStatus.YES:
            case CalendarApp.GuestStatus.MAYBE:
                let alarmTime = _alarmTime(event);
                if (firstAlarmTime == undefined || alarmTime < firstAlarmTime) {
                    firstAlarmIdx = ii;
                    firstAlarmTime = alarmTime;
                }
                break;
            default:
                break;
        }
    }
    if (firstAlarmIdx == undefined) {
        return undefined;
    }
    return events[firstAlarmIdx];
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
                "address": "https://" + devName + ".coertvonk.com:4443/api/push",
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

    const namespaceOID = "{6ba7b812-9dad-11d1-80b4-00c04fd430c9}"
    let channelId = nameToUUID(namespaceOID, e.parameter.devName);
    disablePushNotifications(channelId, e.parameter.pushId);

    // script runs under my account, but publishes Sander's calendar
    //const email = "sander.c.vonk@gmail.com"; // Session.getEffectiveUser().getEmail();
    const email = Session.getEffectiveUser().getEmail();
    let pushId = enablePushNotifications(channelId, e.parameter.devName, email, 60);

    let cal = CalendarApp.getCalendarById(email);
    if (!cal) {
      return ContentService.createTextOutput('no access to calendar (' + email + ')');
    }

    // find the first event today that I'm participating in, and that is not an all day event.

    const now = new Date();
    let todayStart = new Date(); todayStart.setHours(0, 0, 0);  // start at midnight this day
    const oneday = 24*3600000; // [msec]
    const todayStop = new Date(todayStart.getTime() + oneday - 1);
    let eventsToday = cal.getEvents(todayStart, todayStop);
    let firstAlarmToday = _findFirstAlarm(eventsToday);
    
    // find the first event tomorrow that I'm participating in, and that is not an all day event.

    const tomorrowStart = new Date(todayStart.getTime() + oneday);
    const tomorrowStop = new Date(tomorrowStart.getTime() + oneday - 1);
    let eventsTomorrow = cal.getEvents(tomorrowStart, tomorrowStop);
    let firstAlarmTomorrow = _findFirstAlarm(eventsTomorrow);
    
    // select the alarm event that should trigger the alarm

    const event = firstAlarmToday != undefined && _alarmTime(firstAlarmToday) > now ? firstAlarmToday : firstAlarmTomorrow;

    let json = {
        "time": localTime(now),
        "pushId": pushId,
        "events": []
    };
    if (event) {
        json.events.push({
            "start": localTime(event.getStartTime()),
            "end": localTime(event.getEndTime()),
            "title": event.getTitle()
        });
    }

    return ContentService.createTextOutput(JSON.stringify(json)).setMimeType(ContentService.MimeType.JSON);
}