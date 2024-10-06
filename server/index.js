const pg = require("pg");
const express = require("express");
const cors = require("cors");
const app = express();

const config = require("./config.json");

const client = new pg.Client({ database: "activity_tracker" });
app.use(cors());
app.use(express.json());

async function add_log(
  time,
  interval,
  platform,
  left_clicks,
  right_clicks,
  mouse_move,
  keypresses,
) {
  try {
    await client.query("BEGIN");
    const query =
      "insert into logs(time, interval, platform, left_clicks, right_clicks, mouse_move, keypresses) values($1, $2, $3, $4, $5, $6, $7)";
    const values = [
      parseInt(time),
      parseInt(interval),
      parseInt(platform),
      parseInt(left_clicks),
      parseInt(right_clicks),
      parseFloat(mouse_move),
      parseInt(keypresses),
    ];
    const res = await client.query(query, values);
    await client.query("COMMIT");
  } catch (e) {
    await client.query("ROLLBACK");
    throw e;
  }
}

async function get_logs(amt) {
  try {
    const res = await client.query(
      "select * from logs order by time desc limit $1",
      [amt],
    );
    return res.rows;
  } catch (e) {
    console.error("error while querying DB", e);
  }
}

app.post("/add_activity", async (req, res) => {
  if (!req.headers.token || config.token != req.headers.token) return res.sendStatus(400);

  console.log(req.body);
  try {
    await add_log(
      req.body.time,
      req.body.int,
      req.body.pl,
      req.body.lc,
      req.body.rc,
      req.body.mm,
      req.body.kp,
    );
  } catch (e) {
    console.error("error while adding log",e);
    return res.sendStatus(500);
  }
  res.sendStatus(200);
});

app.get("/get_activity", async (req, res) => {
  // last x rows
  const amt = req.query.m;
  if (!amt) return res.sendStatus(400);

  let logs = {};
  (await get_logs(amt)).forEach((l) => {
    logs[l.time] = l;
  });

  res.header("Content-Type", "application/json");
  res.send(JSON.stringify(logs));
});

(async () => {
  await client.connect();

  app.listen(3030, () => {
    console.log("server started");
  });
})();
