"use strict";
const e = require("electron");
console.log("TYPE:", typeof e, "VALUE_SUBSTR:", String(e).substr(0,60));
const app = typeof e === 'object' ? e.app : null;
if (!app) {
  console.error("FAIL: electron.app missing, electron=", String(e).substr(0,80));
  process.exit(1);
}
app.whenReady().then(() => { console.log("SUCCESS - App ready"); app.quit(); });
