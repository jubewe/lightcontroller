const fs = require("fs");

fs.writeFileSync(
  "./index2.html",
  fs.readFileSync("./index.html", "utf-8").replace(/\t|\n(\s)*/g, " ")
);
