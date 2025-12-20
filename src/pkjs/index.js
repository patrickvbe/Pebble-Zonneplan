// Require the keys' numeric values.
var keys = require('message_keys');

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready.');

  // Update s_js_ready on watch
  Pebble.sendAppMessage({JSReady: 1});
});

function fetchStroom(dateint) {
  console.log(dateint);
  var datestr = String(Math.floor(dateint / 10000)) + '-' + String(Math.floor((dateint%10000)/100)) + '-' + String(dateint %100);
  var req = new XMLHttpRequest();
  // https://api.eerlijkverbruik.nl/chart_stroom?date=2025-12-18&company=Zonneplan
  req.open('GET', 'https://api.eerlijkverbruik.nl/chart_stroom?date=' + datestr + '&company=Zonneplan', true);
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        console.log(req.responseText);
        var response = JSON.parse(req.responseText);
        var p1 = response[0][1];
        console.log(p1);
        Pebble.sendAppMessage({Stroom: Math.round(p1*100000)});
      } else {
        console.log('Error');
      }
    }
  };
  req.send(null);
}

Pebble.addEventListener('appmessage', function(e) {
  console.log('Message!');
  console.log(e.type);
  console.log(e.payload[keys.RequestData]);
  fetchStroom(e.payload[keys.RequestData]);
});
