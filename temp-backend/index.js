const express = require("express");
const html = express.static("./frontend");
const { WebSocketServer } = require("ws");

const app = express();

app.listen(80, () => {
  console.log("express listening on port", 80);
});

app.use("/", html);

app.post("/update", (req, res) => {
  console.log("Backend received POST update:", req.body, req.headers);

  res.status(200).send('{"message": "Success"}');
});

const wsServer = new WebSocketServer({
  host: "localhost",
  // server: app,
  // path: "/ws",
  port: 81,
});

wsServer.addListener("listening", () => {
  console.log("WS Server connected");
});

let exampleStates = "1,10,20,30,40,50,60;19,120,130,140,150,160,170;7,120,130,140,150,160,170";

wsServer.addListener("connection", (ws) => {
  ws.send(`refresh;${exampleStates}`);

  ws.onmessage = (msg) => {
    console.log("WS Message", msg.data);

    if (msg === "refresh") {
      ws.send(`refresh;${exampleStates}`);
    }
  };
});

wsServer.addListener("error", (e) => {
  console.error(e);
});

wsServer.addListener("close", () => {
  console.log("WS Server closed");
});
