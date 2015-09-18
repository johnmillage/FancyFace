function fetchWeather(latitude, longitude) {
  var req = new XMLHttpRequest();
  req.open('GET', 'http://api.openweathermap.org/data/2.5/weather?' +
    'lat=' + latitude + '&lon=' + longitude + '&cnt=1&units=imperial&APPID=b8a50ffa21a6f48badf71f6732df166c', true);
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        console.log(req.responseText);
        var response = JSON.parse(req.responseText);
        var temperature = Math.round(response.main.temp);
        var clouds = response.clouds.all;
        console.log(temperature);
        console.log(clouds);
        Pebble.sendAppMessage({
          'PHONE_TEMPERATURE_KEY': temperature + '\xB0',
          'PHONE_TEMPERATURE_C_KEY': Math.round((temperature - 32)/1.8) + '\xB0',
          'PHONE_CLOUDS_KEY': clouds
        });
      } else {
        console.log('Error');
      }
    }
  };
  req.send(null);
}

function locationSuccess(pos) {
  var coordinates = pos.coords;
  fetchWeather(coordinates.latitude, coordinates.longitude);
}

function locationError(err) {
  console.warn('location error (' + err.code + '): ' + err.message);
  Pebble.sendAppMessage({
    'PHONE_TEMPERATURE_KEY': '---',
    'PHONE_TEMPERATURE_C_KEY': '---',
    'PHONE_CLOUDS_KEY': 0
  });
}

var locationOptions = {
  'timeout': 15000,
  'maximumAge': 60000
};

Pebble.addEventListener('ready', function (e) {
  console.log('connect!');
  window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError,
    locationOptions);
  console.log(e.type);
});

Pebble.addEventListener('appmessage', function (e) {
  console.log('message!');
  console.log(e.type);
  console.log(e.payload);
  window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError,
    locationOptions);

  
});

Pebble.addEventListener('showConfiguration', function() {
  var url = 'https://cdn.rawgit.com/johnmillage/FancyFace/f2da62db3d29b3a9c1a3f5bb1c53e61199138cb7/PebbleConfig.html';
  console.log('Showing configuration page: ' + url);

  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
  var configData = JSON.parse(decodeURIComponent(e.response));
  console.log('Configuration page returned: ' + JSON.stringify(configData));

  var backgroundColor = configData['background_color'];
  var tempType = configData['temperature_type'];
  
  var colors = {'All' : 0, 'Darks': 1, 'Lights': 2, 'White': 3, 'Black': 4, 'Blue': 5, 'Yellow': 6, 'Green': 7, 'Red': 8};
    
  var dict = {};
    
    dict['PHONE_COLOR_KEY'] = colors[backgroundColor];
    if(tempType == 'Celsius'){
        dict['PHONE_TEMP_TYPE_KEY'] = 1;
    }
    else{
        dict['PHONE_TEMP_TYPE_KEY'] = 0;
    }
  
    // Send to watchapp
  Pebble.sendAppMessage(dict, function() {
    console.log('Send successful: ' + JSON.stringify(dict));
  }, function() {
    console.log('Send failed!');
  });
});