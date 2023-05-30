const express = require("express");
const html = express.static("./frontend");
const { WebSocketServer } = require("ws");

const app = express();

app.listen(80, () => {
    console.log("express listening on port", 80);
});

app.use("/", html)

app.post("/update", (req, res) => {
    console.log("Backend received POST update:", req.body, req.headers);

    res.status(200).send('{"message": "Success"}');
});

const wsServer = new WebSocketServer({
    host: "localhost",
    // server: app,
    // path: "/ws",
    port: 81
});

wsServer.addListener("listening", () => {
    console.log("WS Server connected");
});

wsServer.addListener("connection", conn => {
    conn.onmessage = (msg) => {
        console.log("WS Message", msg.data);
    };
});

wsServer.addListener("error", e => {
    console.error(e)
});

wsServer.addListener("close", () => {
    console.log("WS Server closed");
});