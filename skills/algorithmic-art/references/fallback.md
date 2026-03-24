# Fallback HTML Structure

Use this if `templates/viewer.html` is not found. This is the minimum correct structure.
Replace all `<!-- VARIABLE -->` sections with art-specific code.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Algorithmic Art</title>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/p5.js/1.7.0/p5.min.js"></script>
  <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@400;500;600&family=Lora:ital@0;1&display=swap" rel="stylesheet">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Poppins', sans-serif;
      background: linear-gradient(135deg, #f5f4ef 0%, #ede9e0 100%);
      display: flex; height: 100vh; overflow: hidden;
    }

    /* Sidebar */
    #sidebar {
      width: 280px; min-width: 280px;
      background: rgba(255,255,255,0.85);
      backdrop-filter: blur(10px);
      border-right: 1px solid rgba(0,0,0,0.08);
      padding: 20px; overflow-y: auto;
      display: flex; flex-direction: column; gap: 20px;
    }
    .sidebar-header { font-size: 18px; font-weight: 600; color: #1a1a1a; }
    .sidebar-header span { font-family: 'Lora', serif; font-style: italic; color: #d97757; }

    .section-title {
      font-size: 11px; font-weight: 600; text-transform: uppercase;
      letter-spacing: 0.08em; color: #888; margin-bottom: 10px;
    }

    /* Seed controls — FIXED, always include */
    .seed-display {
      font-family: monospace; font-size: 20px; font-weight: 600;
      color: #1a1a1a; text-align: center; padding: 8px;
      background: rgba(0,0,0,0.04); border-radius: 6px; margin-bottom: 8px;
    }
    .seed-nav { display: flex; gap: 6px; margin-bottom: 6px; }
    .seed-nav button, .btn {
      flex: 1; padding: 7px; border: 1px solid rgba(0,0,0,0.12);
      border-radius: 6px; background: white; cursor: pointer;
      font-family: 'Poppins', sans-serif; font-size: 12px;
      transition: all 0.15s;
    }
    .btn-primary {
      background: #d97757; color: white; border-color: #d97757; font-weight: 500;
    }
    .seed-jump { display: flex; gap: 6px; }
    .seed-jump input {
      flex: 1; padding: 7px; border: 1px solid rgba(0,0,0,0.12);
      border-radius: 6px; font-family: 'Poppins', sans-serif; font-size: 12px;
    }

    /* Parameter controls — VARIABLE */
    .control-group { margin-bottom: 12px; }
    .control-group label {
      display: flex; justify-content: space-between;
      font-size: 12px; color: #555; margin-bottom: 4px;
    }
    .control-group input[type="range"] { width: 100%; accent-color: #d97757; }
    .control-group input[type="color"] {
      width: 100%; height: 32px; border: none; border-radius: 4px; cursor: pointer;
    }

    /* Canvas area */
    #canvas-container {
      flex: 1; display: flex; align-items: center; justify-content: center;
      padding: 20px; overflow: hidden;
    }
    #canvas-container canvas {
      max-width: 100%; max-height: 100%;
      box-shadow: 0 8px 40px rgba(0,0,0,0.15);
      border-radius: 4px;
    }
  </style>
</head>
<body>
  <div id="sidebar">
    <div class="sidebar-header">Algorithmic Art <br><span><!-- VARIABLE: movement name --></span></div>

    <!-- FIXED: Seed section -->
    <div>
      <div class="section-title">Seed</div>
      <div class="seed-display" id="seed-display">12345</div>
      <div class="seed-nav">
        <button onclick="changeSeed(-1)">← Prev</button>
        <button onclick="randomizeSeed()">Random</button>
        <button onclick="changeSeed(1)">Next →</button>
      </div>
      <div class="seed-jump">
        <input type="number" id="seed-input" placeholder="Seed number">
        <button onclick="jumpToSeed()">Go</button>
      </div>
    </div>

    <!-- VARIABLE: Parameters section -->
    <div>
      <div class="section-title">Parameters</div>
      <!-- Add control-group divs here for each param -->
    </div>

    <!-- VARIABLE: Colors section (include only if needed) -->
    <!--
    <div>
      <div class="section-title">Colors</div>
      <div class="control-group">
        <label>Color 1</label>
        <input type="color" value="#d97757" oninput="updateParam('color1', this.value); regenerate()">
      </div>
    </div>
    -->

    <!-- FIXED: Actions section -->
    <div>
      <div class="section-title">Actions</div>
      <button class="btn btn-primary" onclick="regenerate()" style="width:100%;margin-bottom:6px">Regenerate</button>
      <button class="btn" onclick="resetParams()" style="width:100%;margin-bottom:6px">Reset Defaults</button>
      <button class="btn" onclick="saveCanvas('art-' + params.seed, 'png')" style="width:100%">Download PNG</button>
    </div>
  </div>

  <div id="canvas-container"></div>

  <script>
    // VARIABLE: params object — define for this artwork
    let params = {
      seed: 12345,
      // add artwork-specific parameters
    };

    const defaults = { ...params };

    // FIXED: Seed navigation
    function changeSeed(delta) {
      params.seed += delta;
      if (params.seed < 0) params.seed = 0;
      document.getElementById('seed-display').textContent = params.seed;
      document.getElementById('seed-input').value = '';
      regenerate();
    }
    function randomizeSeed() {
      params.seed = Math.floor(Math.random() * 99999);
      document.getElementById('seed-display').textContent = params.seed;
      regenerate();
    }
    function jumpToSeed() {
      const v = parseInt(document.getElementById('seed-input').value);
      if (!isNaN(v)) { params.seed = v; document.getElementById('seed-display').textContent = v; regenerate(); }
    }

    // VARIABLE: parameter update function
    function updateParam(key, value) {
      params[key] = parseFloat(value) || value;
      // Update display labels if any
    }

    // FIXED: Reset and regenerate
    function resetParams() {
      Object.assign(params, defaults);
      // Re-sync all UI controls to default values
      regenerate();
    }

    function regenerate() {
      // Re-run the sketch with current params
      // For static art: clear and redraw
      // Implementations vary — adapt as needed
    }

    // VARIABLE: p5.js sketch
    new p5(function(p) {
      p.setup = function() {
        let canvas = p.createCanvas(1200, 1200);
        canvas.parent('canvas-container');
        p.randomSeed(params.seed);
        p.noiseSeed(params.seed);
        // Initialize art system
        p.noLoop();
      };

      p.draw = function() {
        // VARIABLE: algorithm goes here
      };
    });
  </script>
</body>
</html>
```

## Key points when using this fallback

- Replace every `<!-- VARIABLE -->` comment with artwork-specific content
- Never change the seed navigation JS functions
- Never change sidebar CSS classes — only add control-group divs inside Parameters
- The `new p5(function(p) {...})` instance mode keeps sketch scoped and avoids global conflicts
- Always pass seed to both `randomSeed` and `noiseSeed` before any random/noise calls
