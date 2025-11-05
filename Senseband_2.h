<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SenseBand Web Dashboard</title>
  <link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body class="bg-gray-100 text-gray-900">
  <nav class="bg-blue-700 text-white p-4 flex justify-between items-center">
    <h1 class="text-xl font-bold">SenseBand Dashboard</h1>
    <div class="text-sm">Connected</div>
  </nav>

  <main class="p-6 grid gap-6 grid-cols-1 md:grid-cols-2 lg:grid-cols-3">
    <!-- Location Card -->
    <div class="bg-white p-4 rounded-2xl shadow-md">
      <h2 class="font-bold text-lg mb-2">Live Location</h2>
      <div id="map" class="h-48 w-full bg-gray-200 flex items-center justify-center">Map Loading...</div>
      <p class="mt-2 text-sm text-gray-500">Lat: <span id="lat">--</span>, Lng: <span id="lng">--</span></p>
    </div>

    <!-- Battery & Signal -->
    <div class="bg-white p-4 rounded-2xl shadow-md">
      <h2 class="font-bold text-lg mb-2">Device Status</h2>
      <p>Battery Level: <span id="battery">--%</span></p>
      <p>Signal Strength: <span id="signal">--</span></p>
      <p>Last Sync: <span id="last-sync">--</span></p>
    </div>

    <!-- Heart Rate -->
    <div class="bg-white p-4 rounded-2xl shadow-md">
      <h2 class="font-bold text-lg mb-2">Heart Rate</h2>
      <p>Current BPM: <span id="bpm">--</span></p>
      <ul id="bpm-log" class="text-sm list-disc pl-4"></ul>
    </div>

    <!-- Obstacle Detection -->
    <div class="bg-white p-4 rounded-2xl shadow-md">
      <h2 class="font-bold text-lg mb-2">Obstacle Alerts</h2>
      <ul id="obstacle-log" class="text-sm list-disc pl-4"></ul>
    </div>

    <!-- Environmental Data -->
    <div class="bg-white p-4 rounded-2xl shadow-md">
      <h2 class="font-bold text-lg mb-2">Environment</h2>
      <p>Temperature: <span id="temp">--</span> °C</p>
      <p>Humidity: <span id="humidity">--</span>%</p>
      <p>Rain Detected: <span id="rain">--</span></p>
    </div>

    <!-- SOS Alerts -->
    <div class="bg-white p-4 rounded-2xl shadow-md">
      <h2 class="font-bold text-lg mb-2">SOS Alerts</h2>
      <ul id="sos-log" class="text-sm list-disc pl-4"></ul>
    </div>

    <!-- Activity Summary Chart -->
    <div class="bg-white p-4 rounded-2xl shadow-md">
      <h2 class="font-bold text-lg mb-2">Activity Summary</h2>
      <canvas id="activityChart" height="200"></canvas>
    </div>

    <!-- Controls -->
    <div class="bg-white p-4 rounded-2xl shadow-md col-span-1 md:col-span-2">
      <h2 class="font-bold text-lg mb-2">Remote Actions</h2>
      <button class="bg-red-600 text-white px-4 py-2 rounded-lg mr-2" onclick="sendSOS()">Trigger SOS</button>
      <button class="bg-blue-600 text-white px-4 py-2 rounded-lg" onclick="sendMessage()">Send Voice Message</button>
    </div>
  </main>

  <script>
    // WebSocket
    const ws = new WebSocket('ws://' + window.location.hostname + ':80/ws');
    ws.onmessage = function(event) {
      const data = JSON.parse(event.data);
      document.getElementById('lat').innerText = data.lat.toFixed(6);
      document.getElementById('lng').innerText = data.lng.toFixed(6);
      document.getElementById('battery').innerText = data.battery + '%';
      document.getElementById('bpm').innerText = data.bpm;
      document.getElementById('signal').innerText = data.signal;
      document.getElementById('last-sync').innerText = data.lastSync;
      const obstacleLog = document.getElementById('obstacle-log');
      if (data.obstacleLog) {
        const li = document.createElement('li');
        li.innerText = data.obstacleLog;
        obstacleLog.prepend(li);
        if (obstacleLog.children.length > 5) obstacleLog.removeChild(obstacleLog.lastChild);
      }
      const sosLog = document.getElementById('sos-log');
      if (data.sosLog) {
        const li = document.createElement('li');
        li.innerText = data.sosLog;
        sosLog.prepend(li);
        if (sosLog.children.length > 5) sosLog.removeChild(sosLog.lastChild);
      }
      const bpmLog = document.getElementById('bpm-log');
      if (data.bpmLog) {
        const li = document.createElement('li');
        li.innerText = data.bpmLog;
        bpmLog.prepend(li);
        if (bpmLog.children.length > 5) bpmLog.removeChild(bpmLog.lastChild);
      }
      document.getElementById('temp').innerText = data.temp;
      document.getElementById('humidity').innerText = data.humidity;
      document.getElementById('rain').innerText = data.rain;
    };

    // Mock data for chart
    const ctx = document.getElementById('activityChart').getContext('2d');
    new Chart(ctx, {
      type: 'line',
      data: {
        labels: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'],
        datasets: [{
          label: 'Steps',
          data: [4000, 5200, 6000, 4800, 7000, 3500, 6200],
          backgroundColor: 'rgba(59, 130, 246, 0.2)',
          borderColor: 'rgba(59, 130, 246, 1)',
          borderWidth: 2
        }]
      },
      options: { responsive: true }
    });

    // Placeholder for remote actions
    function sendSOS() {
      alert('SOS triggered! (This would send an actual command to the device in production)');
    }

    function sendMessage() {
      const msg = prompt("Enter message to send to device:");
      if (msg) alert(`Message sent: "${msg}" (This would be converted to audio on device)`);
    }
  </script>
</body>
</html>