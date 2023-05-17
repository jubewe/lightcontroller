const express = require("express");
const html = express.static("./frontend");
const { WebSocketServer } = require("ws");

const app = express();
const wsServer = new WebSocketServer({
    host: "localhost",
    port: 81
});

wsServer.on("listening", () => {
    console.log("WS Server listening");
});

wsServer.on("connection", conn => {
    conn.onmessage = (msg) => {
        console.log("WS Message", msg.data);
    };
});

app.listen(2222, () => {
    console.log("express listening on port", 80);
});

app.use("/", html)

app.post("/update", (req, res) => {
    console.log("Backend received POST update:", req.body, req.headers);

    res.status(200).send('{"message": "Success"}');
});