const express = require("express");
let html = express.static("./frontend");

const app = express();

app.listen("2222", () => {
    console.log("express listening");
});

app.use("/", html);

app.post("/update", (req, res) => {
    console.log("Backend received POST update:", req.body, req.headers);

    res.status(200).send('{"message": "Success"}');
});