// Switching from the /default/ GCP to a /standard/ GCP project so we can give it permissions
// to the Calendar API and access our domain (calendarclock.coertvonk.com)
//   - in this Google Script > Resources > Cloud Platform project > associate with (new) Cloud project
//   - in Google Cloud Console for that (new) project > OAuth consent screen > make internal, add scope = Google Calendar API ../auth/calendar.readonly
//   - in Google Developers Console > select your project > domain verification > add domain > ..

function generate_uuid() {
    var rand = function(x) {
      if (x <   0) return NaN;
      if (x <= 30) return (0 | Math.random() * (1 <<      x));
      if (x <= 53) return (0 | Math.random() * (1 <<     30))
      + (0 | Math.random() * (1 << x - 30)) * (1 << 30);
      return NaN;
    };
    var hex = function(num, length) {  // _hexAligner
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
    "Authorization":"Bearer " + token,
    "Content-Type": "application/json"
  };
  var options = {
    "method": "POST",
    "headers": header,
    'payload': JSON.stringify(body),
    "muteHttpExceptions": false
  };
  var url = 'https://www.googleapis.com/calendar/v3/channels/stop';

  var response = UrlFetchApp.fetch(url, options);
  Logger.log("responseCode = ", response.getResponseCode());  // 204
  Logger.log("contentText = ", response.getContentText());
}

function enablePushNotifications() {

  var body = {
    "id": generate_uuid(), //"01234567-89ab-cdef-0123456789ab", // your channel UUID
    "type": "web_hook",
    "address": "https://calendarclock.coertvonk.com/api/push",
    "params": {
      "ttl": "3600" // [sec]
    }
  };
  var token = ScriptApp.getOAuthToken();
  var header = {
    "Authorization":"Bearer " + token,
    "Content-Type": "application/json"
  };
  var options = {
    "method":"POST",
    "headers": header,
    'payload': JSON.stringify(body),
    "muteHttpExceptions": false
  };
  var url = 'https://www.googleapis.com/calendar/v3/calendars/sander.c.vonk@gmail.com/events/watch';

  var response = UrlFetchApp.fetch(url, options);

  var data = JSON.parse(response);
  PropertiesService.getScriptProperties().setProperty('resourceId', data.resourceId);

  Logger.log("responseCode = ", response.getResponseCode());  // 200
  Logger.log("contentText = ", response.getContentText());
  Logger.log("resourceId = ", data.resourceId);
}

