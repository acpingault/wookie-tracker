#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Where are you from?</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #1a1a2e;
      color: #eee;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 20px;
    }
    .card {
      background: #16213e;
      border-radius: 14px;
      padding: 36px 32px;
      max-width: 420px;
      width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.5);
      position: relative;
    }
    .gear-btn {
      position: absolute;
      top: 14px;
      right: 14px;
      background: none;
      border: none;
      width: auto;
      padding: 4px;
      color: #4a5568;
      font-size: 1.3rem;
      cursor: pointer;
      line-height: 1;
      transition: color 0.2s;
    }
    .gear-btn:hover { color: #8892a4; background: none; }
    h1 { font-size: 1.6rem; margin-bottom: 6px; }
    .subtitle { color: #8892a4; margin-bottom: 28px; font-size: 0.95rem; }
    label { display: block; margin-bottom: 6px; font-size: 0.85rem; color: #aab; letter-spacing: 0.04em; text-transform: uppercase; }
    input, select {
      width: 100%;
      padding: 11px 14px;
      border: 1px solid #2a3550;
      border-radius: 8px;
      background: #0f2040;
      color: #fff;
      font-size: 1rem;
      margin-bottom: 20px;
      transition: border-color 0.2s;
      outline: none;
    }
    select {
      appearance: none;
      -webkit-appearance: none;
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='12' height='8' viewBox='0 0 12 8'><path fill='%238892a4' d='M6 8L0 0h12z'/></svg>");
      background-repeat: no-repeat;
      background-position: right 14px center;
      padding-right: 38px;
    }
    input:focus, select:focus { border-color: #e94560; }
    button {
      width: 100%;
      padding: 13px;
      background: #e94560;
      border: none;
      border-radius: 8px;
      color: #fff;
      font-size: 1rem;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s;
    }
    button:hover { background: #c73652; }
    button:active { background: #a82d46; }
  </style>
</head>
<body>
  <div class="card">
    <a href="/admin"><button class="gear-btn" type="button" title="Admin">&#9881;</button></a>
    <h1>Where are you from?</h1>
    <p class="subtitle">Light up the board and let us know where the wookies are from!</p>
    <form method="POST" action="/submit">
      <label for="country">Region</label>
      <select id="country" name="country" required autocomplete="country-name" onchange="onRegion(this.value)">
        <option value="" disabled selected>Select your region</option>
        <option>United States</option>
        <option>Canada</option>
        <option>Mexico</option>
        <option>Europe</option>
        <option>Asia</option>
        <option>Africa</option>
        <option>Australia</option>
      </select>

      <input type="hidden" id="state-val" name="state" value="non-us">
      <label for="state-select">State</label>
      <select id="state-select" disabled autocomplete="address-level1" onchange="document.getElementById('state-val').value=this.value">
        <option value="" disabled selected>Select your state</option>
        <option>Alabama</option>
        <option>Alaska</option>
        <option>Arizona</option>
        <option>Arkansas</option>
        <option>California</option>
        <option>Colorado</option>
        <option>Connecticut</option>
        <option>Delaware</option>
        <option>Florida</option>
        <option>Georgia</option>
        <option>Hawaii</option>
        <option>Idaho</option>
        <option>Illinois</option>
        <option>Indiana</option>
        <option>Iowa</option>
        <option>Kansas</option>
        <option>Kentucky</option>
        <option>Louisiana</option>
        <option>Maine</option>
        <option>Maryland</option>
        <option>Massachusetts</option>
        <option>Michigan</option>
        <option>Minnesota</option>
        <option>Mississippi</option>
        <option>Missouri</option>
        <option>Montana</option>
        <option>Nebraska</option>
        <option>Nevada</option>
        <option>New Hampshire</option>
        <option>New Jersey</option>
        <option>New Mexico</option>
        <option>New York</option>
        <option>North Carolina</option>
        <option>North Dakota</option>
        <option>Ohio</option>
        <option>Oklahoma</option>
        <option>Oregon</option>
        <option>Pennsylvania</option>
        <option>Rhode Island</option>
        <option>South Carolina</option>
        <option>South Dakota</option>
        <option>Tennessee</option>
        <option>Texas</option>
        <option>Utah</option>
        <option>Vermont</option>
        <option>Virginia</option>
        <option>Washington</option>
        <option>West Virginia</option>
        <option>Wisconsin</option>
        <option>Wyoming</option>
      </select>

      <button type="submit">Light it up</button>
    </form>
  </div>
  <script>
    function onRegion(val) {
      var sel = document.getElementById('state-select');
      var hidden = document.getElementById('state-val');
      if (val === 'United States') {
        sel.disabled = false;
        hidden.value = sel.value || 'non-us';
      } else {
        sel.disabled = true;
        sel.value = '';
        hidden.value = 'non-us';
      }
    }
  </script>
</body>
</html>
)rawliteral";


const char SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Thanks!</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #1a1a2e;
      color: #eee;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 20px;
    }
    .card {
      background: #16213e;
      border-radius: 14px;
      padding: 36px 32px;
      max-width: 420px;
      width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.5);
      text-align: center;
    }
    .icon { font-size: 3rem; margin-bottom: 16px; }
    h1 { font-size: 1.6rem; margin-bottom: 8px; }
    p { color: #8892a4; margin-bottom: 28px; font-size: 0.95rem; }
    a {
      display: inline-block;
      padding: 11px 28px;
      background: #0f2040;
      border: 1px solid #2a3550;
      border-radius: 8px;
      color: #ccd;
      font-size: 0.95rem;
      text-decoration: none;
      transition: background 0.2s;
    }
    a:hover { background: #1a3060; }
  </style>
</head>
<body>
  <div class="card">
    <div class="icon">&#127881;</div>
    <h1>You're on the map!</h1>
    <p>Thanks for visiting. Your home has been lit up.</p>
    <a href="/">Submit another</a>
  </div>
</body>
</html>
)rawliteral";


const char DUPLICATE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Already submitted</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #1a1a2e;
      color: #eee;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 20px;
    }
    .card {
      background: #16213e;
      border-radius: 14px;
      padding: 36px 32px;
      max-width: 420px;
      width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.5);
      text-align: center;
    }
    .icon { font-size: 3rem; margin-bottom: 16px; }
    h1 { font-size: 1.6rem; margin-bottom: 8px; }
    p { color: #8892a4; font-size: 0.95rem; }
  </style>
</head>
<body>
  <div class="card">
    <div class="icon">&#128205;</div>
    <h1>You're already on the map!</h1>
    <p>Looks like you've already signed this map. Thanks for visiting!</p>
  </div>
</body>
</html>
)rawliteral";


const char ADMIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Admin</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #1a1a2e;
      color: #eee;
      display: flex;
      justify-content: center;
      align-items: flex-start;
      min-height: 100vh;
      padding: 20px;
    }
    .card {
      background: #16213e;
      border-radius: 14px;
      padding: 36px 32px;
      max-width: 480px;
      width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.5);
      margin-top: 40px;
    }
    h1 { font-size: 1.6rem; margin-bottom: 6px; }
    .subtitle { color: #8892a4; margin-bottom: 28px; font-size: 0.95rem; }
    label { display: block; margin-bottom: 6px; font-size: 0.85rem; color: #aab; letter-spacing: 0.04em; text-transform: uppercase; }
    input[type="password"] {
      width: 100%;
      padding: 11px 14px;
      border: 1px solid #2a3550;
      border-radius: 8px;
      background: #0f2040;
      color: #fff;
      font-size: 1rem;
      margin-bottom: 20px;
      outline: none;
      transition: border-color 0.2s;
    }
    input[type="password"]:focus { border-color: #e94560; }
    .btn {
      width: 100%;
      padding: 13px;
      border: none;
      border-radius: 8px;
      color: #fff;
      font-size: 1rem;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s;
    }
    .btn-primary { background: #e94560; }
    .btn-primary:hover { background: #c73652; }
    .btn-danger { background: #7a1c2e; margin-bottom: 24px; }
    .btn-danger:hover { background: #a82d46; }
    .error { color: #e94560; font-size: 0.9rem; margin-bottom: 16px; display: none; }
    .entry-list { list-style: none; }
    .entry-list li {
      padding: 10px 12px;
      border-radius: 6px;
      background: #0f2040;
      margin-bottom: 8px;
      font-size: 0.92rem;
      display: flex;
      justify-content: space-between;
    }
    .entry-list .region { color: #8892a4; font-size: 0.85rem; }
    .count-badge {
      display: inline-block;
      background: #2a3550;
      border-radius: 20px;
      padding: 2px 10px;
      font-size: 0.85rem;
      margin-left: 8px;
      color: #aab;
    }
    .back { display: inline-block; margin-bottom: 20px; color: #8892a4; text-decoration: none; font-size: 0.9rem; }
    .back:hover { color: #eee; }
    .section-title { font-size: 1rem; font-weight: 600; margin-bottom: 12px; display: flex; align-items: center; }
  </style>
</head>
<body>
  <div class="card">
    <a class="back" href="/">&#8592; Back</a>

    <!-- Password gate -->
    <div id="gate">
      <h1>Admin Panel</h1>
      <p class="subtitle">Enter the password to continue.</p>
      <label for="pwd">Password</label>
      <input id="pwd" type="password" placeholder="Password" onkeydown="if(event.key==='Enter')checkPwd()">
      <p id="err" class="error">Incorrect password.</p>
      <button class="btn btn-primary" onclick="checkPwd()">Enter</button>
    </div>

    <!-- Admin panel (hidden until authenticated) -->
    <div id="panel" style="display:none">
      <h1>Admin Panel</h1>
      <p class="subtitle">Manage visitor submissions.</p>

      <button class="btn btn-danger" onclick="clearEntries()">Clear All Entries</button>

      <div class="section-title">
        Submissions <span id="count-badge" class="count-badge">0</span>
      </div>
      <ul id="entry-list" class="entry-list">
        <li style="color:#8892a4">Loading...</li>
      </ul>
    </div>
  </div>
  <script>
    function checkPwd() {
      if (document.getElementById('pwd').value === 'wookiesrule2026') {
        document.getElementById('gate').style.display = 'none';
        document.getElementById('panel').style.display = 'block';
        loadEntries();
      } else {
        document.getElementById('err').style.display = 'block';
      }
    }
    function loadEntries() {
      fetch('/data').then(function(r){ return r.json(); }).then(function(data) {
        document.getElementById('count-badge').textContent = data.count;
        var list = document.getElementById('entry-list');
        if (data.count === 0) {
          list.innerHTML = '<li style="color:#8892a4;justify-content:center">No entries yet.</li>';
          return;
        }
        list.innerHTML = '';
        for (var i = 0; i < data.submissions.length; i++) {
          var s = data.submissions[i];
          var li = document.createElement('li');
          var state = s.state !== 'non-us' ? '<strong>' + s.state + '</strong>' : '';
          var region = s.country;
          li.innerHTML = (state ? state + ' &mdash; ' : '') + '<span class="region">' + region + '</span>';
          list.appendChild(li);
        }
      });
    }
    function clearEntries() {
      if (!confirm('Clear all entries and reset the LED map?')) return;
      fetch('/admin/clear', {method:'POST'}).then(function() {
        loadEntries();
      });
    }
  </script>
</body>
</html>
)rawliteral";
