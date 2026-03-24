/**
 * P5.JS GENERATIVE ART - BEST PRACTICES
 *
 * This file shows STRUCTURE and PRINCIPLES for p5.js generative art.
 * It does NOT prescribe what art to create — your philosophy guides that.
 * Use these patterns for how to structure your code well.
 */

// ─── 1. PARAMETER ORGANIZATION ───────────────────────────────────────────────
// All tunable values in one object. Makes UI connection and reset trivial.

let params = {
    seed: 12345,
    colorPalette: ['#d97757', '#6a9bcc', '#788c5d', '#b0aea5'],
    // Add YOUR parameters here — counts, scales, probabilities, angles, thresholds
};

// ─── 2. SEEDED RANDOMNESS ─────────────────────────────────────────────────────
// Critical: same seed must always produce identical output.

function initializeSeed(seed) {
    randomSeed(seed);
    noiseSeed(seed);
}

// ─── 3. P5.JS LIFECYCLE ───────────────────────────────────────────────────────

function setup() {
    createCanvas(1200, 1200);
    initializeSeed(params.seed);
    // Initialize your system (arrays, grids, starting states)
    // Static art: call noLoop() here
    // Animated art: let draw() run
}

function draw() {
    // Static: generate once, noLoop() prevents redraws
    // Animated: update system each frame
    // User-triggered: noLoop() by default, redraw() on param change
}

// ─── 4. CLASS STRUCTURE ───────────────────────────────────────────────────────
// Use classes for algorithms involving multiple entities (particles, agents, cells)

class Entity {
    constructor() {
        // Use random() here — will be seeded
    }
    update() {
        // Physics, behavioral rules, neighbor interactions
    }
    display() {
        // Rendering — keep separate from update logic
    }
}

// ─── 5. UTILITY FUNCTIONS ────────────────────────────────────────────────────

function hexToRgb(hex) {
    const r = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return r ? { r: parseInt(r[1],16), g: parseInt(r[2],16), b: parseInt(r[3],16) } : null;
}

function colorFromPalette(index) {
    return params.colorPalette[index % params.colorPalette.length];
}

function mapRange(v, inMin, inMax, outMin, outMax) {
    return outMin + (outMax - outMin) * ((v - inMin) / (inMax - inMin));
}

function easeInOutCubic(t) {
    return t < 0.5 ? 4*t*t*t : 1 - Math.pow(-2*t + 2, 3) / 2;
}

// Fade background for trails (alpha < 255 = transparent overlay)
function fadeBackground(alpha) {
    fill(245, 244, 239, alpha);
    noStroke();
    rect(0, 0, width, height);
}

function getNoiseValue(x, y, scale = 0.01) {
    return noise(x * scale, y * scale);
}

function vectorFromAngle(angle, magnitude = 1) {
    return createVector(cos(angle), sin(angle)).mult(magnitude);
}

// ─── 6. PARAMETER UPDATES ────────────────────────────────────────────────────

function updateParameter(paramName, value) {
    params[paramName] = value;
    // Some params update real-time, others need full regeneration
    // Decide per-parameter
}

function regenerate() {
    initializeSeed(params.seed);
    // Reinitialize your generative system
}

// ─── 7. PERFORMANCE NOTES ────────────────────────────────────────────────────
// - Pre-calculate values used in every frame
// - Avoid sqrt/trig in tight loops where possible
// - For 1000+ particles: consider spatial hashing for neighbor queries
// - Target 60fps for animated work; profile if slow
