"use strict";
const electron = require("electron");
console.log("electron type:", typeof electron);
console.log("electron.app type:", typeof (electron.app));
console.log("electron.app:", electron.app ? "EXISTS" : "UNDEFINED");
if (electron.app) {
  electron.app.whenReady().then(() => {
    console.log("App ready!");
    electron.app.quit();
  });
} else {
  console.error("electron.app is undefined. electron value:", String(electron).substring(0,100));
  process.exit(1);
}
