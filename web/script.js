// === TRADINGVIEW WIDGET ===
new TradingView.widget({
  "width": "100%",
  "height": "100%",
  "symbol": "BINANCE:BTCUSDT",
  "interval": "5",
  "timezone": "Etc/UTC",
  "theme": "dark",
  "style": "1",
  "locale": "en",
  "toolbar_bg": "#f1f3f6",
  "enable_publishing": false,
  "hide_top_toolbar": false,
  "hide_legend": false,
  "save_image": false,
  "container_id": "tradingview-widget"
});

// === EEG CHART (Chart.js) ===
const eegCtx = document.getElementById('eeg-canvas').getContext('2d');
const eegChart = new Chart(eegCtx, {
  type: 'line',
  data: {
    labels: Array.from({length: 50}, (_, i) => `${i}s`),
    datasets: [
      {
        label: 'Alpha Waves (Relaxation)',
        data: [],
        borderColor: '#00ff00',
        backgroundColor: 'rgba(0,255,0,0.1)',
        tension: 0.4,
        fill: false
      },
      {
        label: 'Beta Waves (Focus)',
        data: [],
        borderColor: '#ffff00',
        backgroundColor: 'rgba(255,255,0,0.1)',
        tension: 0.4,
        fill: false
      }
    ]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    scales: {
      y: { beginAtZero: true, max: 150 }
    },
    plugins: { legend: { display: true } }
  }
});

// === COLOR SPHERE (Canvas) ===
const sphereCanvas = document.getElementById('colorSphere');
const sphereCtx = sphereCanvas.getContext('2d');
const radius = 100;

function drawSphere(hue = 60) {
  const gradient = sphereCtx.createRadialGradient(100, 100, 0, 100, 100, radius);
  gradient.addColorStop(0, `hsl(${hue}, 100%, 70%)`);
  gradient.addColorStop(1, `hsl(${hue}, 100%, 30%)`);
  
  sphereCtx.clearRect(0, 0, 200, 200);
  sphereCtx.fillStyle = gradient;
  sphereCtx.beginPath();
  sphereCtx.arc(100, 100, radius, 0, 2 * Math.PI);
  sphereCtx.fill();
}

drawSphere(); // Initial draw

// === LIVE EEG DATA FROM C++ SERVER ===
setInterval(async () => {
  try {
    const res = await fetch('http://localhost:8080/api/eeg/latest');
    if (!res.ok) throw new Error("Server not ready");

    const data = await res.json();
    if (!data.TP9) return;

    // Update EEG Chart (push new values)
    const avgSignal = (data.TP9 + data.AF7 + data.AF8 + data.TP10) / 4;

    // Simulate alpha/beta from signal strength (placeholder)
    const alpha = 100 - (avgSignal / 10);
    const beta = avgSignal / 8;

    eegChart.data.labels.shift();
    eegChart.data.labels.push(new Date().toLocaleTimeString());

    eegChart.data.datasets[0].data.shift();
    eegChart.data.datasets[0].data.push(alpha);

    eegChart.data.datasets[1].data.shift();
    eegChart.data.datasets[1].data.push(beta);

    eegChart.update('quiet');

    // Update Color Sphere: High beta = red, high alpha = green
    const stress = beta / (alpha + beta || 1);
    const hue = 120 - (stress * 120); // 120 (green) → 0 (red)
    drawSphere(hue);

  } catch (err) {
    console.log("Waiting for EEG server...", err.message);
  }
}, 200);