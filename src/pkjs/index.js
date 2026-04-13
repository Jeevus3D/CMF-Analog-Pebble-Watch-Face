// WMO code → simplified icon code:
// 0=clear, 1=cloudy, 2=rain, 3=snow, 4=storm
function wmoToCode(wmo) {
  if (wmo === 0)                          return 0; // clear
  if (wmo <= 3)                           return 1; // cloudy
  if ((wmo >= 51 && wmo <= 67) ||
      (wmo >= 80 && wmo <= 82))           return 2; // rain
  if ((wmo >= 71 && wmo <= 77) ||
      (wmo >= 85 && wmo <= 86))           return 3; // snow
  if (wmo >= 95)                          return 4; // storm
  return 1; // default cloudy
}

function fetchWeather(lat, lon) {
  var url = 'https://api.open-meteo.com/v1/forecast' +
    '?latitude='          + lat +
    '&longitude='         + lon +
    '&current_weather=true' +
    '&temperature_unit=fahrenheit' +
    '&wind_speed_unit=mph';

  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    try {
      var data  = JSON.parse(this.responseText);
      var temp  = Math.round(data.current_weather.temperature);
      var code  = wmoToCode(data.current_weather.weathercode);
      Pebble.sendAppMessage(
        { 'TEMPERATURE': temp, 'WEATHER_CODE': code },
        function() { console.log('Weather sent: ' + temp + '° code:' + code); },
        function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
      );
    } catch(e) { console.log('Parse error: ' + e); }
  };
  xhr.open('GET', url);
  xhr.send();
}

function getLocation() {
  navigator.geolocation.getCurrentPosition(
    function(pos) { fetchWeather(pos.coords.latitude, pos.coords.longitude); },
    function(err) { console.log('Location error: ' + err.message); },
    { timeout: 15000, maximumAge: 300000 }
  );
}

Pebble.addEventListener('ready',      function() { getLocation(); });
Pebble.addEventListener('appmessage', function() { getLocation(); });