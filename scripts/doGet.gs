//  Fetch all events to show on clock
//  Platform: Google WebApp Script
//  (c) Copyright 2020, Sander Vonk

function generate_uuid() {
    var rand = function (x) {
        if (x < 0) return NaN;
        if (x <= 30) return (0 | Math.random() * (1 << x));
        if (x <= 53) return (0 | Math.random() * (1 << 30))
            + (0 | Math.random() * (1 << x - 30)) * (1 << 30);
        return NaN;
    };
    var hex = function (num, length) {  // _hexAligner
        var str = num.toString(16), i = length - str.length, z = "0";
        for (; i > 0; i >>>= 1, z += z) { if (i & 1) { str = z + str; } }
        return str;
    };
    return hex(rand(32), 8) + "-" + hex(rand(16), 4) + "-" + hex(0x4000 | rand(12), 4) + "-" +
        hex(0x8000 | rand(14), 4) + "-" + hex(rand(48), 12);
};

function disablePushNotifications() {
    var resourceId = PropertiesService.getScriptProperties().getProperty('resourceId');
    Logger.log("resourceId = ", resourceId);
    var body = {
        "id": "01234567-89ab-cdef-0123456789ab", // Your channel ID
        "resourceId": resourceId
    };
    var token = ScriptApp.getOAuthToken();
    var header = {
        "Authorization": "Bearer " + token,
        "Content-Type": "application/json"
    };
    var options = {
        "method": "POST",
        "headers": header,
        'payload': JSON.stringify(body),
        "muteHttpExceptions": true //was false
    };
    var url = 'https://www.googleapis.com/calendar/v3/channels/stop';
    var response = UrlFetchApp.fetch(url, options);
}

function enablePushNotifications() {
    var body = { "id": "01234567-89ab-cdef-0123456789ab", "type": "web_hook", "address": "https://calendarclock.coertvonk.com/api/push", "params": { "ttl": "320" } }; //time in seconds
    var token = ScriptApp.getOAuthToken();
    var header = { "Authorization": "Bearer " + token, "Content-Type": "application/json" };
    var options = { "method": "POST", "headers": header, 'payload': JSON.stringify(body), "muteHttpExceptions": true };
    var url = 'https://www.googleapis.com/calendar/v3/calendars/sander.c.vonk@gmail.com/events/watch';
}

function doGet() {
    disablePushNotifications()
    enablePushNotifications()
    var cal = CalendarApp.getCalendarById('sander.c.vonk@gmail.com');
    // Logger.log(Session.getActiveUser().getEmail());
    //var cal = CalendarApp.getDefaultCalendar()
    if (cal == undefined) { return ContentService.createTextOutput("no access to calendar"); }

    var todayStart = new Date();
    const oneDay = 24 * 3600000;  // Convert to milliseconds for calculations
    todayStart.setHours(0, 0, 0); // Start at midnight today
    const todayStop = new Date(todayStart.getTime() + oneDay - 1);
    const tomorrowStart = new Date(todayStart.getTime() + oneDay);
    const tomorrowStop = new Date(tomorrowStart.getTime() + oneDay - 1);
    const now = new Date();
    const inHalfDay = new Date(now.getTime() + (oneDay / 2) - 1)

    var eventsToday = cal.getEvents(todayStart, todayStop);
    var eventsTomorrow = cal.getEvents(tomorrowStart, tomorrowStop);
    var events = eventsToday.concat(eventsTomorrow)
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
                }
            }
            case CalendarApp.GuestStatus.NO:
            case CalendarApp.GuestStatus.INVITED:
                break;
        }
    }
    for (var qq = 0; qq < eventStartRaw.length; qq++) {
        if ((eventStartRaw[qq] != eventStartRaw[qq - 1]) && (eventEndRaw[qq] != eventEndRaw[qq - 1])) {
            raw.push({ "start": eventStartRaw[qq], "stop": eventEndRaw[qq] })
        }
    }

    const correctedTime = new Date(now.getTime() - 25200000)
    const myJSON = JSON.stringify({ "time": correctedTime, "events": raw });
    return ContentService.createTextOutput(myJSON).setMimeType(ContentService.MimeType.JSON);
}