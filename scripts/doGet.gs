/*  Fetch all events to show on clock
 *  Platform: Google WebApp Script
 *  (c) Copyright 2020, Sander Vonk
 *
 *  Events are now filtered on:
 *   1)starting before time in 12h
 *   2)ending after current time
 *   3)not having an undefined time
 *   4)Having a response of Owner, Yes, or Maybe
 *
 *  Then they are modified so that:
 *   1)The start time is at least at the current time
 *   2)The end time is at the very most in 12 hours
 *   3)If neither is met, the event is spliced/deleted
 *   4)Time is set to # of milliseconds after top of clock time
 *
 *  To-Do or Features that need to be implemented
 *   4)Implementation for switching to hourly view and back
*/
function doGet() {
  var time = new Date();
  if (time.getHours() < 12) {time.setHours(0, 0, 0, 0)}
  else {time.setHours(12, 0, 0, 0)}
  var topOfClockTime = time.getTime()  // Push this value as a Epoch time
  var cal = CalendarApp.getCalendarById('sander.c.vonk@gmail.com'); // Open the calender and assign it to var 'cal'
  if (cal == undefined) {return ContentService.createTextOutput("no access to calendar");}
  var todayStart = new Date();
  const oneDay = 24 * 3600000;  // Convert to milliseconds for caculations
  todayStart.setHours(0, 0, 0); // Start at midnight today
  const todayStop = new Date(todayStart.getTime() + oneDay - 1);
  const tomorrowStart = new Date(todayStart.getTime() + oneDay);
  const tomorrowStop = new Date(tomorrowStart.getTime() + oneDay - 1);
  const now = new Date();
  const inHalfDay = new Date(now.getTime() + (oneDay / 2) - 1)
  var eventsToday = cal.getEvents(todayStart, todayStop);
  var eventsTomorrow = cal.getEvents(tomorrowStart, tomorrowStop);
  var events = eventsToday.concat(eventsTomorrow)
  // Creates arrays for start and stop times of events that will be caculated and .push ed
  var eventStarts = [], eventEnds = []
  for (var ii = 0; ii < events.length; ii++) {
    var event = events[ii];
    var dgb = events[ii].getTitle()
    switch (event.getMyStatus()) {
      case CalendarApp.GuestStatus.OWNER:
      case CalendarApp.GuestStatus.YES:
      case CalendarApp.GuestStatus.MAYBE: {
        // Filters on events that would actually show on the clock face if shown
        const allDayEvent = event.getStartTime() == undefined
        const didntEndYet = event.getEndTime() < inHalfDay
        const stillHasToStart = event.getStartTime() > now
        if (!allDayEvent && (stillHasToStart || didntEndYet)) {
          var eventIndex = ii;
          eventStarts.push(event.getStartTime().getTime())
          eventEnds.push(event.getEndTime().getTime())
        }
      }
      case CalendarApp.GuestStatus.NO:
      case CalendarApp.GuestStatus.INVITED:
        break;
    }
  }
  var str = [], hueList = []
  var baseHue = (360.0 / eventStarts.length)
  for (var tt = 0, jj = 0; tt < eventStarts.length; tt++) {
    if ((eventEnds[tt] < now.getTime()) && (eventStarts[tt] < now.getTime())) {
      eventEnds.splice(tt, 1)
      eventStarts.splice(tt, 1)
    } else {
      if (eventStarts[tt] < now.getTime()) {eventStarts[tt] = now.getTime()}
      if (eventEnds[tt] > inHalfDay.getTime()) {eventEnds[tt] = inHalfDay.getTime()}
    }
    // Sets the event start and stop times for each array object to the time in minutes from the start of the current 12h segment
    eventStarts[tt] = (eventStarts[tt] - topOfClockTime) / 60000
    eventEnds[tt] = (eventEnds[tt] - topOfClockTime) / 60000
    // Brings the variable(s) we are working with from an array and to single variables with which we can easily work with
    var endPixel = (Math.round(eventEnds[tt] / 12) % 60), startPixel = (Math.round(eventStarts[tt] / 12 % 60))
    if ((endPixel <= 0) || (startPixel <= 0)) {
      continue;
    }
    hueList.push (Math.round(baseHue * (jj)))
    var hue = hueList[jj]
    jj++
    // Since the RGB led ring is has 60 pixels and the array to define them starts at 0, those at 60 need to be moved to a value of 0
    if (startPixel == 60) {startPixel = 0}
    else if (eventStarts[tt] < 0) {eventStarts[tt] += 60}
    if (endPixel == 60) { endPixel = 0}
    // Pushes current modified/filtered variables to the array 'str' (don't ask) to be turned into JSON elements.
    // Those that wrap around the top of the clock/led ring are split into two managable pieces (one ending at 59 and one starting at 0)
    if (startPixel >= endPixel) {
      str.push({ "first": startPixel, "last": 59, "hue": hue})
      str.push({ "first": 0, "last": endPixel, "hue": hue})
    }
    // Others are simpily pushed in the correct format to the 'str' array
    else {str.push({"first": startPixel, "last": endPixel, "hue" : hue})}
  } // End of for loop
  const seriouslyThisBetterWork = new Date(now.getTime() - 25200000)
  const myJSON = JSON.stringify({"events": str,"time": seriouslyThisBetterWork});
  return ContentService.createTextOutput(myJSON).setMimeType(ContentService.MimeType.JSON);
}