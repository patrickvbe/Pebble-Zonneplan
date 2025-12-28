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
      const buffer = new ArrayBuffer((STROOM_TARIEF_COUNT+3) * 4);
      const data = new Uint32Array(buffer);
      data[2] = 0;
      if (req.status === 200) {
        try {
          //console.log(req.responseText);
          var response = JSON.parse(req.responseText);
          if ( response.length >= 4 ) {
            tarieven = response[0];
            belasting = response[1];
            gemiddeld = response[2];
            inkoopvergoeding = response[3];
            data[0] = dateint;
            data[1] = belasting;
            if ( tarieven.length > 0 && tarieven[0] != '' ) {
              data[2] = STROOM_TARIEF_COUNT;
              for ( pointidx = 0; pointidx < STROOM_TARIEF_COUNT; pointidx++) {
                if ( pointidx < tarieven.length ) data[pointidx+3] = (Math.round((tarieven[pointidx]+inkoopvergoeding)*1.21*100000));
                else data[pointidx] = 0;
              }
            }
          }
        } catch(error) {}
      }
      Pebble.sendAppMessage({Stroom: Array.from(new Uint8Array(buffer))});
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
