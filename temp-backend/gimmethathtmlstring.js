const fs = require("fs");

console.log(fs.readFileSync("../frontend/index.html", "utf-8").replace(/\t|\n(\s)*/g, " "));