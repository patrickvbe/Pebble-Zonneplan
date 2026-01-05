// Show energy tariffs on the Pebble watch.
// Copyright (C) 2026 Patrick van Beem (patrick@vanbeem.info)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


// Require the keys' numeric values.
var keys = require('message_keys');

// Import the Clay package for the configuration page
var Clay = require('pebble-clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// Initialize Clay
var clay = new Clay(clayConfig);

const STROOM_TARIEF_COUNT = 24;

Pebble.addEventListener('ready', function() {
  //console.log('PebbleKit JS ready.');

  // Update s_js_ready on watch
  Pebble.sendAppMessage({JSReady: 1});
});

function fetchStroom(dateint) {
  var datestr = String(Math.floor(dateint / 10000)) + '-' + String(Math.floor((dateint%10000)/100)) + '-' + String(dateint %100);
  var req = new XMLHttpRequest();
  // https://www.stroomperuur.nl/ajax/tarieven.php?datum=2025-12-27
  req.open('GET', 'https://www.stroomperuur.nl/ajax/tarieven.php?datum=' + datestr, true);
  req.onload = function () {
    //console.log("OnLoad" + req.readyState);
    if (req.readyState === 4) {
      const buffer = new ArrayBuffer((STROOM_TARIEF_COUNT+3) * 4);
      const data = new Uint32Array(buffer);
      data[2] = 0;
      //console.log(req.status);
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
              //console.log("Expected data");
              data[2] = STROOM_TARIEF_COUNT;
              for ( pointidx = 0; pointidx < STROOM_TARIEF_COUNT; pointidx++) {
                // if ( pointidx < tarieven.length ) data[pointidx+3] = (Math.round((tarieven[pointidx]+inkoopvergoeding)*1.21*100000));
                if ( pointidx < tarieven.length ) data[pointidx+3] = tarieven[pointidx]*100000;
                else data[pointidx] = 0;
              }
            } else {
              //console.log("Unexpected data");
            }
          }
        } catch(error) {
          console.log("Error:");
          console.log(error);
        }
      }
      //console.log("Send response");
      Pebble.sendAppMessage({Stroom: Array.from(new Uint8Array(buffer))});
    }
  };
  req.send(null);
}

Pebble.addEventListener('appmessage', function(e) {
  console.log(e.type);
  console.log(e.payload[keys.RequestData]);
  fetchStroom(e.payload[keys.RequestData]);
});
