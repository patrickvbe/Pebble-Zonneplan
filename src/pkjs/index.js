// Require the keys' numeric values.
var keys = require('message_keys');

const STROOM_TARIEF_COUNT = 24;

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready.');

  // Update s_js_ready on watch
  Pebble.sendAppMessage({JSReady: 1});
});

function fetchStroom(dateint) {
  console.log(dateint);
  var datestr = String(Math.floor(dateint / 10000)) + '-' + String(Math.floor((dateint%10000)/100)) + '-' + String(dateint %100);
  var req = new XMLHttpRequest();
  // // https://api.eerlijkverbruik.nl/chart_stroom?date=2025-12-18&company=Zonneplan
  // req.open('GET', 'https://api.eerlijkverbruik.nl/chart_stroom?date=' + datestr + '&company=Zonneplan', true);
  // req.onload = function () {
  //   console.log("OnLoad");
  //   if (req.readyState === 4) {
  //     console.log(req.status);
  //     if (req.status === 200) {
  //       // console.log(req.responseText);
  //       console.log(3);
  //       var response = JSON.parse(req.responseText);
  //       console.log(2);
  //       const buffer = new ArrayBuffer(STROOM_TARIEF_COUNT * 4);
  //       console.log(1);
  //       const data = new Uint32Array(buffer);
  //       console.log(response.length);
  //       for ( pointidx = 0; pointidx < STROOM_TARIEF_COUNT; pointidx++) {
  //         if ( pointidx < response.length ) data[pointidx] = (Math.round(response[pointidx][1]*100000));
  //         else data[pointidx] = 0;
  //       }
  //       console.log('Response');
  //       Pebble.sendAppMessage({Stroom: Array.from(new Uint8Array(buffer))});
  //     } else {
  //       console.log('Error');
  //     }
  //   }
  // };
  // https://www.stroomperuur.nl/ajax/tarieven.php?leverancier=2&datum=2025-12-27
  req.open('GET', 'https://www.stroomperuur.nl/ajax/tarieven.php?leverancier=2&datum=' + datestr, true);
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        //console.log(req.responseText);
        var response = JSON.parse(req.responseText);
        const buffer = new ArrayBuffer(STROOM_TARIEF_COUNT * 4);
        const data = new Uint32Array(buffer);
        if ( response.length >= 4 ) {
          tarieven = response[0];
          belasting = response[1];
          gemiddeld = response[2];
          inkoopvergoeding = response[3];
          for ( pointidx = 0; pointidx < STROOM_TARIEF_COUNT; pointidx++) {
            if ( pointidx < tarieven.length ) data[pointidx] = (Math.round((tarieven[pointidx]+inkoopvergoeding)*1.21*100000));
            else data[pointidx] = 0;
          }
          Pebble.sendAppMessage({Stroom: Array.from(new Uint8Array(buffer))});
        }
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
