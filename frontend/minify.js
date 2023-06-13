const fs = require("fs");
const { minify } = require("html-minifier-terser");

(async () => {
    const html = fs.readFileSync("./index.html", "utf-8");
    const mini = await minify(html, {
        minifyCSS: true,
        minifyJS: true,
        removeTagWhitespace: true,
        collapseWhitespace: true
    });
    fs.writeFileSync("./indexmini.html", mini, "utf-8");
})().catch(console.error);