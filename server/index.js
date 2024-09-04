const pg = require("pg");
const express = require("express");
const cors = require("cors");
const app = express();
const { formidable } = require("formidable");

const config = {
  token: "asd",
};
const client = new pg.Client({ database: "activity_tracker" });
app.use(cors());

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
    throw e;
  }
}

app.post("/add_activity", async (req, res) => {
  if (config.token != req.headers.token) res.sendStatus(400);

  const form = formidable({});

  form.parse(req, async (err, fields, files) => {
    if (err) {
      return res.sendStatus(500);
    }

    try {
      await add_log(
        fields.time,
        fields.int,
        fields.pl,
        fields.lc,
        fields.rc,
        fields.mm,
        fields.kp,
      );
    } catch (e) {
      console.log(e);
      return res.sendStatus(500);
    }
    res.sendStatus(200);
  });
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

  app.listen(3030, () => {});
})();
