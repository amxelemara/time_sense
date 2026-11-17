var Clay = require('@rebble/clay');
var clayConfig = require('./config');

var clay = new Clay(clayConfig);

// Settings can also be changed on the watch, so ask the watch for its current values and store
// them where Clay reads its page defaults. Otherwise the phone page would show stale values.
Pebble.addEventListener('ready', function() {
  Pebble.sendAppMessage({ RequestSettings: 1 }, null, function() {
    console.log('Could not request settings from the watch');
  });
});

Pebble.addEventListener('appmessage', function(e) {
  var p = e.payload;
  if (p.Interval === undefined) {
    return;
  }
  clay.setSettings({
    Enabled: !!p.Enabled,
    Interval: String(p.Interval),
    Vibe: String(p.Vibe),
    HourVibe: String(p.HourVibe)
  });
  console.log('Settings from watch: ' + localStorage.getItem('clay-settings'));
});
